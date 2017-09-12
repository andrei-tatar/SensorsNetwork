import { Subject } from 'rxjs/Subject';
import { Observable } from 'rxjs/Observable';
import { MessageLayer, ConnectableMessageLayer } from './interfaces';
import * as _ from 'lodash';
import 'rxjs/add/operator/map';
import 'rxjs/add/operator/filter';
import 'rxjs/add/operator/first';
import 'rxjs/add/operator/timeout';
import 'rxjs/add/operator/toPromise';
import 'rxjs/add/operator/takeWhile';

export class Communication {
    private readonly keys: Buffer[] = [];
    private readonly rxMessages: Observable<{ msg: Buffer, id: number }>;

    constructor(private below: MessageLayer) {
        this.rxMessages = this.below.data
            .filter(p => p[0] === 0x93)
            .map(p => {
                const msg = new Buffer(p.length - 2);
                p.copy(msg, 0, 2, p.length);
                return { msg, id: p[1] };
            });
    }

    private async sendPacketAndWaitFor(packet: Buffer, verify: (packet: Buffer) => boolean, timeout: number = 700) {
        const waitPromise = this.below.data
            .map(p => verify(p))
            .filter(s => s)
            .first()
            .timeout(timeout)
            .toPromise();
        await this.below.send(packet);
        await waitPromise;
    }

    private async send(id: number, data: Buffer) {
        if (id >= this.keys.length)
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

    register(key: Buffer): ConnectableMessageLayer {
        if (key.length != 16) throw new Error('invalid key length');

        let closed = false;
        let prevSend: Promise<void>;

        return {
            open: async () => {
                this.keys.push(key);
                await this.init();
            },
            close: async () => {
                closed = true;
                _.pull(this.keys, key);
                await this.init();
            },
            data: this.rxMessages
                .takeWhile(t => !closed)
                .filter(m => m.id === _.findIndex(this.keys, key))
                .map(m => m.msg),
            send: async (msg) => {
                if (closed) throw new Error('sending data on a closed channel');
                const id = _.findIndex(this.keys, key);
                if (prevSend) await prevSend;
                prevSend = this.send(id, msg);
                return prevSend;
            },
        }
    }

    private async init() {
        let data = new Buffer(this.keys.length * 16 + 2);
        let offset = data.writeUInt8(0x90, 0);
        offset = data.writeUInt8(this.keys.length, offset);
        for (const key of this.keys) {
            offset += key.copy(data, offset, 0, 16);
        }

        await this.sendPacketAndWaitFor(data, p => p.length == 1 && p[0] == 0x91);
    }
}
