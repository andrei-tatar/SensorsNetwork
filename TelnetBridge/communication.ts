import * as net from 'net';
import * as _ from 'lodash';

enum RxStatus {
    Idle, Header, Size, Data, Checksum1, Checksum2
}

const FrameHeader1 = 0xDE, FrameHeader2 = 0x5B;

export class Communication {
    private readonly client: net.Socket;
    private readonly connectionPromise: Promise<void>;
    private readonly waiters: ((packet: Buffer) => boolean)[] = [];
    private readonly nodes: { key: Buffer, callback: (msg: Buffer) => void }[] = [];

    private status = RxStatus.Idle;
    private rxBuffer: Buffer;
    private rxOffset: number;
    private rxChecksum: number;

    constructor(private host: string) {
        this.client = new net.Socket();
        this.connectionPromise = new Promise((resolve, reject) => {
            this.client.once('error', err => reject(err));
            this.client.connect(23, host, function () {
                console.log('connected!');
                resolve();
            });
        });

        this.client.on('data', data => this.handleIncomingData(data));
    }

    private handleIncomingData(rx: Buffer) {
        for (const data of rx) {
            switch (this.status) {
                case RxStatus.Idle:
                    if (data == FrameHeader1) this.status = RxStatus.Header;
                    break;
                case RxStatus.Header:
                    this.status = data == FrameHeader2 ? RxStatus.Size : RxStatus.Idle;
                    break;
                case RxStatus.Size:
                    this.rxBuffer = new Buffer(data + 1);
                    this.rxOffset = this.rxBuffer.writeUInt8(data, 0);
                    this.status = RxStatus.Data;
                    break;
                case RxStatus.Data:
                    this.rxOffset = this.rxBuffer.writeUInt8(data, this.rxOffset);
                    if (this.rxOffset == this.rxBuffer.length) this.status = RxStatus.Checksum1;
                    break;
                case RxStatus.Checksum1:
                    this.rxChecksum = data << 8;
                    this.status = RxStatus.Checksum2;
                    break;
                case RxStatus.Checksum2:
                    this.rxChecksum |= data;
                    const checksum = this.getChecksum(this.rxBuffer, 0, this.rxBuffer.length);
                    if (checksum === this.rxChecksum) {
                        const packet = new Buffer(this.rxBuffer.length - 1);
                        this.rxBuffer.copy(packet, 0, 1, this.rxBuffer.length);
                        this.onPacketReceived(packet);
                    }
                    this.status = RxStatus.Idle;
                    break;

                default:
                    break;

            }
        }
    }

    private onPacketReceived(packet: Buffer) {
        if (packet[0] === 0x93) {
            const id = packet[1];
            if (id >= this.nodes.length) return;

            const msg = new Buffer(packet.length - 2);
            packet.copy(msg, 0, 2, packet.length);
            this.nodes[id].callback(msg);
            return;
        }

        for (const waiter of this.waiters) {
            if (waiter(packet)) return;
        }
        console.log('unhandled rx', packet);
    }

    private getChecksum(data: Buffer, offset: number, length: number) {
        let checksum = 0x1021;

        for (let i = 0; i < length; i++) {
            const byte = data[offset + i];
            const roll = (checksum & 0x8000) != 0 ? true : false;
            checksum <<= 1;
            checksum &= 0xFFFF;
            if (roll) checksum |= 1;
            checksum ^= byte;
        }

        return checksum;
    }

    private async waitFor(verify: (packet: Buffer) => boolean, timeout: number) {
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                _.pull(this.waiters, waiter);
                reject(new Error('timeout; no response'));
            }, timeout);

            const waiter = (packet) => {
                let finished = false;
                let error;
                try {
                    if (verify(packet)) finished = true;
                } catch (err) {
                    error = err;
                    finished = true;
                }

                if (finished) {
                    _.pull(this.waiters, waiter);
                    clearTimeout(timer);
                    if (error) reject(error); else resolve();
                }

                return finished;
            };

            this.waiters.push(waiter);
        });
    }

    private sendPacket(data: Buffer) {
        const packet = new Buffer(data.length + 5);
        let offset = packet.writeUInt8(FrameHeader1, 0);
        offset = packet.writeUInt8(FrameHeader2, offset);
        offset = packet.writeUInt8(data.length, offset);
        offset += data.copy(packet, offset, 0, data.length);
        const checksum = this.getChecksum(packet, 2, data.length + 1);
        packet.writeUInt16BE(checksum, offset);

        return new Promise<void>(resolve => this.client.write(packet, resolve));
    }

    private async sendPacketAndWaitFor(packet: Buffer, verify: (packet: Buffer) => boolean, timeout: number = 700) {
        const waitPromise = this.waitFor(verify, timeout);
        await this.sendPacket(packet);
        await waitPromise;
    }

    private async send(id: number, data: Buffer) {
        await this.connectionPromise;

        if (id >= this.nodes.length)
            throw new Error('invalid id');

        const packet = new Buffer(data.length + 2);
        let offset = packet.writeUInt8(0x92, 0);
        offset = packet.writeUInt8(id, offset);
        data.copy(packet, offset, 0, data.length);
        await this.sendPacketAndWaitFor(packet, r => {
            if (r[0] === 0x94 && r[1] === id) return true; //packet sent
            if (r[0] === 0x70 && r[1] === id) throw new Error('id busy (TODO: should retry)');
            if (r[0] === 0x71 && r[1] === id) throw new Error('invalid packet size');
            if (r[0] === 0x72 && r[1] === id) throw new Error('invalid id');
            if (r[0] === 0x73 && r[1] === id) throw new Error('timeout; no ACK');
        });
    }

    async register(key: Buffer, listener: (msg: Buffer) => void): Promise<Registration> {
        if (key.length != 16) throw new Error('invalid key length');

        this.nodes.push({ key, callback: listener });
        await this.init();

        let closed = false;
        let prevSend: Promise<void>;
        return {
            close: async () => {
                closed = true;
                _.remove(this.nodes, { key });
                await this.init();
            },
            send: async (msg) => {
                if (closed) throw new Error('sending data on a closed node');
                const id = _.findIndex(this.nodes, { key });
                if (prevSend) await prevSend;
                prevSend = this.send(id, msg);
                return prevSend;
            },
        }
    }

    private async init() {
        let data = new Buffer(this.nodes.length * 16 + 2);
        let offset = data.writeUInt8(0x90, 0);
        offset = data.writeUInt8(this.nodes.length, offset);
        for (const node of this.nodes) {
            offset += node.key.copy(data, offset, 0, 16);
        }

        await this.sendPacketAndWaitFor(data, p => p.length == 1 && p[0] == 0x91);
    }
}

export interface Registration {
    send(msg: Buffer): Promise<void>;
    close(): Promise<void>;
}