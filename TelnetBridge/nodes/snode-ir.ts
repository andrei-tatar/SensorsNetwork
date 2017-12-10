import { Decoder } from './../decoders/decoder';
import { Observable } from 'rxjs/Observable';
import { Subscription } from 'rxjs/Subscription';
import { NodeRedNode } from './../communication/interfaces';
import { PackageCommunication } from '../communication/packagecommunication';

import 'rxjs/add/operator/do';
import 'rxjs/add/operator/timestamp';
import * as moment from 'moment';

module.exports = function (RED) {

    function IrNode(config) {
        const decoder = new Decoder();

        RED.nodes.createNode(this, config);
        const node: NodeRedNode = this;

        const bridge = RED.nodes.getNode(config.bridge);
        if (!bridge) return;

        const connected: Observable<boolean> = bridge.connected;
        const pckg: PackageCommunication = bridge.package;
        const state = pckg.data
            .map(msg => {
                if (msg[0] === 1) {
                    const pulses: number[] = [];
                    for (let i = 1; i < msg.length; i++) {
                        pulses.push(msg[i] * 50);
                    }
                    return decoder.decode(pulses);
                }
                return null;
            })
            .filter(msg => !!msg)
            .do(state => {
                node.send({ payload: state })
            })
            .timestamp();

        node.on('input', msg => {
            if (typeof msg.payload !== 'string') return;

            const pulses = decoder.encode(msg.payload);
            if (!pulses) return;

            const buffer = Buffer.from([1, ...pulses.map(p => Math.round(p / 50))]);
            pckg.send(buffer).catch(err => node.error(`while sending code: ${err.message}`));;
        });

        const subscription =
            Observable
                .combineLatest(connected, state.startWith(null), Observable.interval(30000).startWith(0))
                .subscribe(([connected, state]) => {
                    const lastMessage = state ? `(${moment(state.timestamp).fromNow()})` : '';
                    node.status(connected
                        ? { fill: 'green', shape: 'dot', text: `connected ${lastMessage}` }
                        : { fill: 'red', shape: 'ring', text: 'not connected' });
                });

        node.on('close', () => subscription.unsubscribe());
    }

    RED.nodes.registerType('snode-ir', IrNode);
};
