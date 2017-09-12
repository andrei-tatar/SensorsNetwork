import { Subject } from 'rxjs/Subject';
import { MessageLayer } from './interfaces';
import { Telnet } from './telnet';

const FrameHeader1 = 0xDE, FrameHeader2 = 0x5B;

enum RxStatus {
    Idle, Header, Size, Data, Checksum1, Checksum2
}

export class PackageCommunication implements MessageLayer {
    private status = RxStatus.Idle;
    private rxBuffer: Buffer;
    private rxOffset: number;
    private rxChecksum: number;
    private packetReceived: Subject<Buffer>;
    
    constructor(private below: MessageLayer) {
        this.packetReceived = new Subject();
        below.data.subscribe(data => this.processReceivedData(data));
    }

    private processReceivedData(rx: Buffer) {
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
                        this.packetReceived.next(packet);
                    }
                    this.status = RxStatus.Idle;
                    break;

                default:
                    break;

            }
        }
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

    async send(data: Buffer) {
        const packet = new Buffer(data.length + 5);
        let offset = packet.writeUInt8(FrameHeader1, 0);
        offset = packet.writeUInt8(FrameHeader2, offset);
        offset = packet.writeUInt8(data.length, offset);
        offset += data.copy(packet, offset, 0, data.length);
        const checksum = this.getChecksum(packet, 2, data.length + 1);
        packet.writeUInt16BE(checksum, offset);

        await this.below.send(packet);
    }

    get data() {
        return this.packetReceived.asObservable();
    }
}