import { Observable } from 'rxjs/Observable';
import { Subscription } from "rxjs/Subscription";
import { Subject } from 'rxjs/Subject';
import { BehaviorSubject } from 'rxjs/BehaviorSubject';
import { ConnectableMessageLayer, Logger } from './interfaces';
import 'rxjs/add/operator/distinctUntilChanged';
import 'rxjs/add/observable/interval';
import * as net from 'net';
import * as _ from 'lodash';

export class Telnet implements ConnectableMessageLayer {
    private static readonly baudRate = 57600;

    private readonly _data: Subject<Buffer>;
    private socket: net.Socket;
    private reconnectTimeout: NodeJS.Timer;
    private _connected = new BehaviorSubject<boolean>(false);
    private hearbeat: Subscription;

    constructor(private host: string, private port: number, private logger: Logger, private reconnectInterval: number = 5000) {
        this._data = new Subject<Buffer>();
        this.connected.subscribe(connected => {
            if (connected) {
                this.hearbeat = Observable.interval(5000).subscribe(s => {
                    try {
                        this.sendRaw(Buffer.from([1, 2, 3])); //heartbeat
                    } catch (err) {
                        logger.warn(`could not send heartbeat ${err.message}`);
                    }
                });
            } else {
                if (this.hearbeat)
                    this.hearbeat.unsubscribe();
            }
        });
    }

    get data() {
        return this._data.asObservable();
    }

    get connected() {
        return this._connected.asObservable().distinctUntilChanged();
    }

    open() {
        if (this.socket) {
            this.socket.destroy();
            this.socket = null;
        }

        this.socket = new net.Socket();
        this.socket.on('data', data => this._data.next(data));
        this.socket.once('disconnect', () => {
            this._connected.next(false);
            this.logger.warn('telnet: disconnected from server');
            this.reconnectTimeout = setTimeout(() => this.open(), this.reconnectInterval);
        });
        this.socket.once('error', err => {
            this._connected.next(false);
            this.logger.warn(`telnet: error: ${err.message}`);
            this.reconnectTimeout = setTimeout(() => this.open(), this.reconnectInterval);
        });

        this.logger.debug('telnet: connecting');
        this.socket.connect(this.port, this.host, async () => {
            this.logger.debug('telnet: connected');
            await this.setBaud(Telnet.baudRate);
            this._connected.next(true);
        });
    }

    close() {
        this._connected.next(false);
        this._connected.complete();
        this._data.complete();
        clearTimeout(this.reconnectTimeout);
        if (this.socket) {
            this.socket.destroy();
            this.socket = null;
        }
    }

    async send(data: Buffer) {
        if (!this.socket) throw new Error('telnet: socket not initialized');

        const connected = await this._connected.first().toPromise();
        if (!connected) throw new Error('telnet: not connected');

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

    private sendRaw(data: Buffer) {
        return new Promise<void>(resolve => this.socket.write(data, resolve));
    }

    private async setBaud(baud: number) {
        let changeBaud = Buffer.from([
            0xFF, 0xFB, 0x2C,

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
        changeBaud.writeUInt32BE(baud, 7);
        await this.sendRaw(changeBaud);
    }
}
