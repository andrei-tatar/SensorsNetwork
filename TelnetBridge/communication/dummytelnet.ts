import { Subject } from 'rxjs/Subject';
import { logger } from './../logger';
import { ConnectableMessageLayer } from './interfaces';

export class DummyTelenet implements ConnectableMessageLayer {
    private _data: Subject<Buffer>;

    constructor() {
        this._data = new Subject();
        this._data.subscribe(d => logger.info('rsp: ', d));
    }

    close() {
    }

    async open(): Promise<void> {
    }

    get data() {
        return this._data.asObservable();
    }

    async send(data: Buffer): Promise<void> {
        logger.info('snd: ', data);
        if (data[3] == 0x90) setTimeout(() => this._data.next(Buffer.from([0xDE, 0x5B, 0x01, 0x91, 0x40, 0x17])), 100) //init ok!

        // if (data[3] == 0x92) setTimeout(() => this._data.next(Buffer.from([0xDE, 0x5B, 0x02, 0x94, data[4], 0x80, 0x28 ^ data[4]])), 100);
        if (data[3] == 0x92) setTimeout(() => this._data.next(Buffer.from([0xde, 0x5b, 0x02, 0x73, data[4], 0x81, 0xe6 ^ data[4]])), 500);
    }
}