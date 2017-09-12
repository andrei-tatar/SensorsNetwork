import { Subject } from 'rxjs/Subject';
import { Observable } from 'rxjs/Observable';
import { MessageLayer, ConnectableMessageLayer } from './interfaces';
import * as _ from 'lodash';
import 'rxjs/add/operator/map';
import 'rxjs/add/operator/filter';
import 'rxjs/add/operator/first';
import 'rxjs/add/operator/timeout';
import 'rxjs/add/operator/toPromise';
import 'rxjs/add/operator/catch';

class CommunicationError extends Error {
    constructor(message: string) {
        super(message);
    }
}

class RetryError extends CommunicationError {
    constructor(message: string) {
        super(message);
    }
}

class TimeoutNoAckError extends RetryError {
    constructor(message: string) {
        super(message);
    }
}

export class Communication {
    private readonly keys: Buffer[] = [];
    private readonly rxMessages: Observable<{ msg: Buffer, id: number }>;
    private inited: boolean;
    private initPromise = _.memoize(() => this.init());

    private static readonly Cmd_Init = 0x90;
    private static readonly Cmd_SendMsg = 0x92;
    private static readonly Rsp_Inited = 0x91;
    private static readonly Rsp_MsgRecevied = 0x93;
    private static readonly Rsp_MsgSent = 0x94;
    private static readonly Err_NodeBusy = 0x70;
    private static readonly Err_InvalidSize = 0x71;
    private static readonly Err_InvalidId = 0x72;
    private static readonly Err_NoAck = 0x73;

    constructor(private below: MessageLayer) {
        this.rxMessages = this.below.data
            .filter(p => p[0] === Communication.Rsp_MsgRecevied && p[1] < this.keys.length)
            .map(p => {
                const msg = new Buffer(p.length - 2);
                p.copy(msg, 0, 2, p.length);
                return { msg, id: p[1] };
            });
    }

    private async sendPacketAndWaitFor(packet: Buffer, verifyReply: (packet: Buffer) => boolean, tries: number = 3, timeout: number = 600) {
        while (tries-- !== 0) {
            try {
                const waitPromise = this.below.data
                    .map(p => verifyReply(p))
                    .filter(s => s)
                    .first()
                    .timeout(timeout)
                    .catch(err => {
                        if (err instanceof CommunicationError) throw err;
                        if (err.message.indexOf('Timeout') >= 0) throw new RetryError('timeout; no reply received');
                        throw err;
                    })
                    .toPromise()
                await this.below.send(packet);
                await waitPromise;
                break;
            } catch (err) {
                if (err instanceof RetryError && tries !== 0) continue;
                throw err;
            }
        }
    }

    private async send(id: number, data: Buffer) {
        if (id < 0 || id >= this.keys.length)
            throw new Error('invalid id');

        const packet = new Buffer(data.length + 2);
        let offset = packet.writeUInt8(Communication.Cmd_SendMsg, 0);
        offset = packet.writeUInt8(id, offset);
        data.copy(packet, offset, 0, data.length);

        await this.sendPacketAndWaitFor(packet, r => {
            if (r[0] === Communication.Rsp_MsgSent && r[1] === id) return true; //packet sent
            if (r[0] === Communication.Err_NoAck && r[1] === id) throw new TimeoutNoAckError('timeout; no ACK');
            if (r[0] === Communication.Err_NodeBusy && r[1] === id) throw new RetryError('id busy');
            if (r[0] === Communication.Err_InvalidSize && r[1] === id) throw new CommunicationError('invalid packet size');
            if (r[0] === Communication.Err_InvalidId && r[1] === id) throw new CommunicationError('invalid id');
        });
    }

    private getNodeIndex(key: Buffer) {
        return _.indexOf(this.keys, key);
    }

    register(key: Buffer): MessageLayer {
        if (key.length != 16) throw new Error('invalid key length');
        if (this.inited) throw new Error('cannot register after init');

        let prevSend: Promise<void>;
        this.keys.push(key);

        return {
            data: this.rxMessages
                .filter(m => m.id === this.getNodeIndex(key))
                .map(m => m.msg),
            send: async (msg) => {
                await this.initPromise();
                if (prevSend) await prevSend.catch(e => { });
                prevSend = this.send(this.getNodeIndex(key), msg);
                return prevSend;
            },
        }
    }

    private async init() {
        if (this.inited) return;

        let data = new Buffer(this.keys.length * 16 + 2);
        let offset = data.writeUInt8(Communication.Cmd_Init, 0);
        offset = data.writeUInt8(this.keys.length, offset);
        for (var key of this.keys) {
            offset += key.copy(data, offset, 0, 16);
        }

        await this.sendPacketAndWaitFor(data, p => p.length == 1 && p[0] == Communication.Rsp_Inited);
        this.inited = true;
    }
}
