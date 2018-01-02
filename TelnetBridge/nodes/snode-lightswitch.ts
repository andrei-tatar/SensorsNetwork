import { NodeRedNode } from './../communication/interfaces';
import { Subscription } from 'rxjs/Subscription';
import { Observable } from 'rxjs/Observable';
import { Communication } from './../communication/communication';
import * as moment from 'moment';
import 'rxjs/add/observable/combineLatest'
import 'rxjs/add/operator/startWith'
import 'rxjs/add/operator/first'

module.exports = function (RED) {

    const cmd_SetState = 0x01;
    const cmd_GetState = 0x02;
    const cmd_SetMode = 0x03;
    const rsp_Init = 0x01;
    const rsp_State = 0x02;

    function LightSwitchNode(config) {
        RED.nodes.createNode(this, config);
        const node: NodeRedNode = this;

        const bridge = RED.nodes.getNode(config.bridge);
        if (!bridge) return;

        const key = Buffer.from(config.key, 'hex');
        const communication: Communication = bridge.communication;
        const connected: Observable<boolean> = bridge.connected;
        const nodeLayer = communication.register(key);
        const subscriptions: Subscription[] = [];
        const periodicSync = Observable.interval(5 * 60000).startWith(0);

        subscriptions.push(Observable.combineLatest(connected, periodicSync)
            .subscribe(async ([connected, index]) => {
                if (!connected) return;
                try {
                    await nodeLayer.send(Buffer.from([cmd_GetState]));
                    await nodeLayer.send(Buffer.from([cmd_SetMode, config.manual ? 0 : 1]));
                } catch (err) {
                    node.error(`while sync: ${err.message}`)
                }
            }));

        subscriptions.push(
            Observable
                .combineLatest(connected, nodeLayer.data.startWith(null).timestamp(), Observable.interval(30000).startWith(0))
                .subscribe(([connected, msg]) => {
                    const lastMessage = msg ? `(${moment(msg.timestamp).fromNow()})` : '';

                    node.status(connected
                        ? { fill: 'green', shape: 'dot', text: `connected ${lastMessage}` }
                        : { fill: 'red', shape: 'ring', text: 'not connected' });
                }));

        let state = 0;

        subscriptions.push(
            nodeLayer.data
                .subscribe(msg => {
                    switch (msg[0]) {
                        case rsp_Init:
                            nodeLayer.send(Buffer.from([cmd_SetMode, config.manual ? 0 : 1]));
                            break;
                        case rsp_State:
                            state = msg[1];
                            var states: { channel: number, state: boolean }[] = [];
                            for (let i = 0; i < config.channels; i++) {
                                node.send({
                                    payload: (state & (1 << i)) === 0 ? false : true,
                                    topic: config.topic + i,
                                });
                            }
                            break;
                    }
                }));

        node.on('input', msg => {
            for (let i = 0; i < config.channels; i++) {
                if (msg.topic === config.topic + i) {
                    const value = !!msg.payload;
                    if (value) state |= 1 << i; else state &= ~(1 << i);
                    nodeLayer.send(Buffer.from([cmd_SetState, state])).catch(err => node.error(`while setting state: ${err.message}`));;
                    return;
                }
            }
        });

        node.on('close', () => {
            subscriptions.forEach(s => s.unsubscribe());
            nodeLayer.close();
        });
    }

    RED.nodes.registerType('snode-lightswitch', LightSwitchNode);
}