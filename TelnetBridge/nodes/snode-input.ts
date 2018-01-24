import { Observable } from 'rxjs/Observable';
import { Subscription } from 'rxjs/Subscription';
import { NodeRedNode } from './../communication/interfaces';
import { Communication } from './../communication/communication';
import 'rxjs/add/operator/do';
import 'rxjs/add/operator/timestamp';
import * as moment from 'moment';

module.exports = function (RED) {

    function InputNode(config) {
        RED.nodes.createNode(this, config);
        const node: NodeRedNode = this;

        const bridge = RED.nodes.getNode(config.bridge);
        if (!bridge) return;

        const key = Buffer.from(config.key, 'hex');
        const connected: Observable<boolean> = bridge.connected;
        const communication: Communication = bridge.communication;
        const nodeLayer = communication.register(key);
        const state = nodeLayer.data
            .map(msg => {
                const state: { [key: string]: any } = {};
                for (let offset = 0; offset < msg.length; offset++) {
                    switch (msg.readUInt8(offset)) {
                        case 'B'.charCodeAt(0):
                            state.voltage = msg.readUInt8(offset + 1) / 100 + 1; //volts
                            offset += 1;
                            break;
                        case 'T'.charCodeAt(0):
                            state.temperature = msg.readInt16BE(offset + 1) / 256; //deg C
                            offset += 2;
                            break;
                        case 'L'.charCodeAt(0):
                            let lux_high = msg.readUInt8(offset + 1);
                            let lux_low = msg.readUInt8(offset + 2);
                            const lux_exponent = ((lux_high >> 4) & 0x0F);
                            lux_high = ((lux_high << 4) & 0xF0);
                            lux_low &= 0x0F;
                            state.light = 0.045 * (lux_high + lux_low) * (1 << lux_exponent); //lux
                            offset += 2;
                            break;
                        case 'S'.charCodeAt(0):
                            state.pir = msg.readUInt8(offset + 1) !== 0; //true/false
                            offset += 1;
                            break;
                        case 'H'.charCodeAt(0):
                            state.humidity = msg.readUInt16BE(offset + 1) / 100; // %
                            offset += 2;
                            break;
                        case 'P'.charCodeAt(0):
                            state.pressure = msg.readUInt16BE(offset + 1) / 10; //hPa
                            offset += 2;
                            break;
                    }
                }
                return state;
            })
            .do(state => {
                node.send({ payload: state })
            })
            .timestamp();

        const subscription =
            Observable
                .combineLatest(connected, state.startWith(null), Observable.interval(30000).startWith(0))
                .subscribe(([connected, state]) => {
                    const lastMessage = state ? `(${moment(state.timestamp).fromNow()})` : '';
                    node.status(connected
                        ? { fill: 'green', shape: 'dot', text: `connected ${lastMessage}` }
                        : { fill: 'red', shape: 'ring', text: 'not connected' });
                });

        node.on('close', () => {
            subscription.unsubscribe();
            nodeLayer.close();
        });
    }

    RED.nodes.registerType('snode-input', InputNode);
}