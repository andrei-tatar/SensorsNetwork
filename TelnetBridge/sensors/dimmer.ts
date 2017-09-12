import { MessageLayer } from './../communication/interfaces';
import { ReplaySubject } from 'rxjs/ReplaySubject';

export class Dimmer {
    private readonly _brightness: ReplaySubject<number>;

    private static readonly cmd_SetBrightness = 0x01;
    private static readonly cmd_GetStatus = 0x02;
    private static readonly cmd_SetLedBrightness = 0x03;

    private static readonly rsp_BrightnessLevel = 0x02;

    constructor(private layer: MessageLayer) {
        this._brightness = new ReplaySubject(1);
    }

    get brightness() {
        return this._brightness.asObservable();
    }

    async requestStateUpdate() {
        await this.layer.send(Buffer.from([0x02]));
    }

    protected onMessage(msg: Buffer) {
        switch (msg[0]) {
            case Dimmer.rsp_BrightnessLevel: this._brightness.next(msg[1]);
        }
    }

    async setBrightness(value: number) {
        await this.layer.send(Buffer.from([Dimmer.cmd_SetBrightness, value]));
    }

    async setLedBrightness(value: number) {
        await this.layer.send(Buffer.from([Dimmer.cmd_SetLedBrightness, value]));
    }
}