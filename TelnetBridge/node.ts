import { MessageLayer, ConnectableMessageLayer } from './communication/interfaces';
import { Communication } from './communication/communication';

export class Node {
    private readonly registration: ConnectableMessageLayer;

    constructor(key: Buffer, comm: Communication) {
        this.registration = comm.register(key);
        this.registration.then(() => this.init());
    }

    protected async init() {
    }

    protected onMessage(msg: Buffer) {
    }

    protected async send(msg: Buffer) {
        await (await this.registration).send(msg);
    }

    async close() {
        await (await this.registration).close();
    }
}
