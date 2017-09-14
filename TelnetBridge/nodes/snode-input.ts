import { Observable } from 'rxjs/Observable';
import { Subscription } from 'rxjs/Subscription';
import { NodeRedNode } from './../communication/interfaces';
import { Communication } from './../communication/communication';

import 'rxjs/add/operator/do';

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
            .filter(msg => msg.length === 3)
            .map(msg => {
                const state = msg.readUInt8(0);
                const voltage = msg.readUInt16BE(1) / 1000;
                return { state, voltage };
            })
            .do(state => {
                node.send({ payload: state })
            });

        const subscription =
            Observable
                .combineLatest(connected, state.startWith(null))
                .subscribe(([connected, state]) => {
                    node.status(connected
                        ? { fill: 'green', shape: 'dot', text: `connected${state ? ` (${state.state}, ${Math.round(state.voltage * 10) / 10}V)` : ''}` }
                        : { fill: 'red', shape: 'ring', text: 'not connected' });
                });

        node.on('close', () => {
            subscription.unsubscribe();
            nodeLayer.close();
        });
    }

    RED.nodes.registerType('snode-input', InputNode);
}