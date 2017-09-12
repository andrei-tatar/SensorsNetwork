import { Communication, Registration } from './communication';

export class Node {
    private readonly registration: Promise<Registration>;

    constructor(key: Buffer, comm: Communication) {
        this.registration = comm.register(key, msg => this.onMessage(msg));
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
