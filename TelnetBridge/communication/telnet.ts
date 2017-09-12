import { logger } from './../logger';
import { ConnectableMessageLayer } from './interfaces';
import { Subject } from 'rxjs/Subject';
import * as net from 'net';
import * as _ from 'lodash';

export class Telnet implements ConnectableMessageLayer {
    private socket: net.Socket;
    private readonly _data: Subject<Buffer>;

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
            this.socket.on('disconnect', () => {
                logger.warn('telnet: disconnected from server');
                setTimeout(() => this.open(), 5000);
            });
            this.socket.once('error', err => {
                logger.warn('telnet: error: ', err.message);
                setTimeout(() => this.open(), 5000);
            });

            logger.debug('telnet: connecting');
            this.socket.connect(this.port, this.host, () => {
                logger.debug('telnet: connected');
                resolve();
            });
        });
    }

    close(): Promise<void> {
        throw new Error("Method not implemented.");
    }

    send(data: Buffer) {
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
        return new Promise<void>(resolve => this.socket.write(escapedData, resolve));
    }

    get data() {
        return this._data.asObservable();
    }
}
