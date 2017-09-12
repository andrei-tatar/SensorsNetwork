import { BehaviorSubject } from 'rxjs/BehaviorSubject';
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
import 'rxjs/add/operator/groupBy';
import 'rxjs/add/operator/concatMap';
import 'rxjs/add/operator/mergeall';

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

interface QueueMessage {
    msg: Buffer;
    id: number;
    resolve: (value?) => void;
    reject: (err) => void;
}

export class Communication {
    private readonly nodes: { key: Buffer, id?: number }[] = [];
    private readonly rxMessages: Observable<{ msg: Buffer, id: number }>;
    private nodesChanged: boolean = false;
    private readonly txQueue: Subject<QueueMessage>;

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
            .filter(p => p[0] === Communication.Rsp_MsgRecevied)
            .map(p => {
                const msg = new Buffer(p.length - 2);
                p.copy(msg, 0, 2, p.length);
                return { msg, id: p[1] };
            });
        this.txQueue = new Subject();
        this.txQueue
            .groupBy(m => m.id)
            .map(group =>
                group.concatMap(msg => {
                    let subject = new BehaviorSubject(msg);
                    let oldReject = msg.reject;
                    let oldResolve = msg.resolve;
                    msg.reject = (err) => { oldReject(err); subject.complete(); };
                    msg.resolve = () => { oldResolve(); subject.complete(); };
                    return subject;
                })
            )
            .mergeAll()
            .subscribe(({ id, msg, resolve, reject }) => {
                const data = new Buffer(msg.length + 2);
                let offset = data.writeUInt8(Communication.Cmd_SendMsg, 0);
                offset = data.writeUInt8(id, offset);
                msg.copy(data, offset, 0, msg.length);

                this.sendPacketAndWaitFor(data,
                    r => {
                        if (r[0] === Communication.Rsp_MsgSent && r[1] === id) return true; //packet sent
                        if (r[0] === Communication.Err_NoAck && r[1] === id) throw new TimeoutNoAckError('timeout; no ACK');
                        if (r[0] === Communication.Err_NodeBusy && r[1] === id) throw new RetryError('id busy');
                        if (r[0] === Communication.Err_InvalidSize && r[1] === id) throw new CommunicationError('invalid packet size');
                        if (r[0] === Communication.Err_InvalidId && r[1] === id) throw new CommunicationError('invalid id');
                    })
                    .then(() => resolve())
                    .catch(err => reject(err));
            });
    }

    private async sendPacketAndWaitFor(packet: Buffer, verifyReply: (packet: Buffer) => boolean, tries: number = 3, timeout: number = 600) {
        while (tries--) {
            try {
                await this.below.send(packet);
                await this.below.data
                    .map(p => verifyReply(p))
                    .filter(s => s)
                    .first()
                    .timeout(timeout)
                    .catch(err => {
                        if (err instanceof CommunicationError) throw err;
                        if (err.message.indexOf('Timeout') >= 0) throw new RetryError('timeout; no reply received');
                        throw err;
                    })
                    .toPromise();
                break;
            } catch (err) {
                if (err instanceof RetryError && tries !== 0) continue;
                throw err;
            }
        }
    }

    private async send(id: number, msg: Buffer) {
        if (id < 0)
            throw new Error('invalid id');


        await new Promise((resolve, reject) => {
            this.txQueue.next({ id, msg, resolve, reject });
        });
    }

    register(key: Buffer): ConnectableMessageLayer {
        if (key.length != 16) throw new Error('invalid key length');

        const node = { key, id: null };
        this.nodes.push(node);
        this.nodesChanged = true;

        return {
            close: () => {
                node.id = null;
                _.pull(this.nodes, node);
                this.nodesChanged = true;
            },
            data: this.rxMessages
                .filter(m => m.id === node.id)
                .map(m => m.msg),
            send: async (msg) => {
                await this.init();
                await this.send(node.id, msg);
            },
        }
    }

    async init(force: boolean = false) {
        if (!force && !this.nodesChanged) return;

        const data = new Buffer(this.nodes.length * 16 + 2);
        let offset = data.writeUInt8(Communication.Cmd_Init, 0);
        offset = data.writeUInt8(this.nodes.length, offset);

        _.forEach(this.nodes, (node, index) => {
            offset += node.key.copy(data, offset, 0, 16);
            node.id = index;
        });

        await this.sendPacketAndWaitFor(data, p => p.length == 1 && p[0] == Communication.Rsp_Inited);

        this.nodesChanged = false;
    }

    close() {
        this.txQueue.complete();
    }
}
