import { Subscription } from 'rxjs/Subscription';
import { MessageLayer } from './../communication/interfaces';
import { ReplaySubject } from 'rxjs/ReplaySubject';

export class Dimmer {
    private readonly _brightness: ReplaySubject<number>;
    private readonly subscription: Subscription;
    private static readonly cmd_SetBrightness = 0x01;
    private static readonly cmd_GetStatus = 0x02;
    private static readonly cmd_SetMode = 0x03;
    private static readonly cmd_SetLedBrightness = 0x04;

    private static readonly rsp_Init = 0x01;
    private static readonly rsp_BrightnessLevel = 0x02;

    private readonly state = {
        mode: null,
        ledBrightness: null,
    }

    constructor(private layer: MessageLayer) {
        this._brightness = new ReplaySubject(1);
        this.subscription = layer.data.subscribe(s => this.onMessage(s));
    }

    close() {
        this.subscription.unsubscribe();
    }

    get brightness() {
        return this._brightness.asObservable();
    }

    async requestStateUpdate() {
        await this.layer.send(Buffer.from([0x02]));
    }

    async setMode(manualControl: boolean, manualDimm: boolean, manualBrightness: number) {
        let mode: number = 0;
        if (manualDimm) mode |= 0x01;
        if (!manualControl) mode |= 0x02;

        const setMode = Buffer.from([Dimmer.cmd_SetMode, mode, manualBrightness]);
        this.state.mode = setMode;
        await this.layer.send(setMode);
    }

    protected onMessage(msg: Buffer) {
        switch (msg[0]) {
            case Dimmer.rsp_Init:
                this.syncState();
                break;
            case Dimmer.rsp_BrightnessLevel: this._brightness.next(msg[1]);
        }
    }

    async setBrightness(value: number) {
        const brightness = Buffer.from([Dimmer.cmd_SetBrightness, value]);
        await this.layer.send(brightness);
    }

    async setLedBrightness(value: number) {
        const ledBrightness = Buffer.from([Dimmer.cmd_SetLedBrightness, value]);
        this.state.ledBrightness = ledBrightness;
        await this.layer.send(ledBrightness);
    }

    private syncState() {
        this.requestStateUpdate().catch(err => { });
        if (this.state.mode) this.layer.send(this.state.mode).catch(err => { });
        if (this.state.ledBrightness) this.layer.send(this.state.ledBrightness).catch(err => { });
    }
}