import { logger } from './../logger';
import { ConnectableMessageLayer } from './interfaces';
import { Subject } from 'rxjs/Subject';
import * as net from 'net';
import * as _ from 'lodash';

export class Telnet implements ConnectableMessageLayer {
    private readonly _data: Subject<Buffer>;
    private socket: net.Socket;
    private reconnectTimeout: NodeJS.Timer;

    constructor(private host: string, private port: number) {
        this._data = new Subject<Buffer>();
    }

    open() {
        return new Promise<void>((resolve, reject) => {
            if (this.socket) {
                this.socket.destroy();
                this.socket = null;
            }

            this.socket = new net.Socket();
            this.socket.on('data', data => this._data.next(data));
            this.socket.once('disconnect', () => {
                logger.warn('telnet: disconnected from server');
                this.reconnectTimeout = setTimeout(() => this.open(), 5000);
            });
            this.socket.once('error', err => {
                logger.warn('telnet: error: ', err.message);
                this.reconnectTimeout = setTimeout(() => this.open(), 5000);
            });

            logger.debug('telnet: connecting');
            this.socket.connect(this.port, this.host, async () => {
                logger.debug('telnet: connected');
                await this.setBaud(57600);
                resolve();
            });
        });
    }

    async close() {
        clearTimeout(this.reconnectTimeout);
        if (this.socket) {
            this.socket.end();
            this.socket = null;
        }
    }

    async send(data: Buffer) {
        if (!this.socket) throw new Error('Layer not initialized');

        const bytesToEscape = _.countBy(data, byte => byte === 0xFF).true || 0;
        const escapedData = new Buffer(data.length + bytesToEscape);
        let offset = 0;
        for (const byte of data) {
            offset = escapedData.writeUInt8(byte, offset);
            if (byte == 0xFF) {
                offset = escapedData.writeUInt8(byte, offset);
            }
        }

        await this.sendRaw(escapedData);
    }

    get data() {
        return this._data.asObservable();
    }

    private sendRaw(data: Buffer) {
        return new Promise<void>(resolve => this.socket.write(data, resolve));
    }

    private async setBaud(baud: number) {
        let changeBaud = Buffer.from([
            0xFF, //IAC
            0xFA, //command sequence begin
            0x2C, //port options
            0x01, //set baud
            0x00, //baud b1
            0x00, //baud b2
            0x00, //baud b3
            0x00, //baud b4
            0xFF, //IAC
            0xF0, //command sequence end
        ]);
        changeBaud.writeUInt32BE(baud, 4);
        console.log(changeBaud);
        await this.sendRaw(changeBaud);
    }
}
