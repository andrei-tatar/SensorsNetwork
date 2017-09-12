import { Communication } from './communication';
import { Node } from './node';

export class Dimmer extends Node {
    private _brightness: number;

    constructor(key: Buffer, comm: Communication) {
        super(key, comm);

    }

    protected async init() {
        await this.send(Buffer.from([0x02]));
    }

    protected onMessage(msg: Buffer) {
        switch (msg[0]) {
            case 0x02: this._brightness = msg[1];
        }
    }

    async setBrightness(value: number) {
        await this.send(Buffer.from([0x01, value]))
    }

    async setLetBrightness(value: number) {
        await this.send(Buffer.from([0x03, value]))
    }
}