import { NodeRedNode } from './../communication/interfaces';
import { Subscription } from 'rxjs/Subscription';
import { Observable } from 'rxjs/Observable';
import { Dimmer } from './../sensors/dimmer';
import { Communication } from './../communication/communication';

import 'rxjs/add/observable/combineLatest'
import 'rxjs/add/operator/startWith'
import 'rxjs/add/operator/first'
import 'rxjs/add/operator/distinctUntilChanged'

module.exports = function (RED) {

    function DimmerNode(config) {
        RED.nodes.createNode(this, config);
        const node: NodeRedNode = this;

        const bridge = RED.nodes.getNode(config.bridge);
        if (!bridge) return;

        const key = Buffer.from(config.key, 'hex');
        const communication: Communication = bridge.communication;
        const connected: Observable<boolean> = bridge.connected;
        const nodeLayer = communication.register(key);
        const dimmer = new Dimmer(nodeLayer);

        const subscriptions: Subscription[] = [];

        subscriptions.push(connected
            .subscribe(connected => {
                if (connected) {
                    dimmer.requestStateUpdate().catch(err => node.error(`while getting status: ${err.message}`));
                    dimmer.setMode(config.manual, config.manualdimm, config.maxbrightness).catch(err => node.error(`while setting mode: ${err.message}`));
                    dimmer.setLedBrightness(config.ledbrightness).catch(err => node.error(`while setting led bright: ${err.message}`));
                }
            }));

        subscriptions.push(
            Observable
                .combineLatest(connected, dimmer.brightness.startWith(0))
                .subscribe(([connected, brightness]) => {
                    node.status(connected
                        ? { fill: 'green', shape: 'dot', text: `connected (${brightness ? brightness + '%' : 'off'})` }
                        : { fill: 'red', shape: 'ring', text: 'not connected' });
                }));

        subscriptions.push(
            dimmer.brightness
                .distinctUntilChanged()
                .subscribe(brightness => {
                    node.send({
                        payload: {
                            brightness
                        }
                    });
                }));

        node.on('input', msg => {
            if (typeof msg.payload.brightness === 'number') {
                dimmer.setBrightness(msg.payload.brightness).catch(err => node.error(`while setting bright: ${err.message}`));;
            }
            if (typeof msg.payload.ledbrightness === 'number') {
                config.ledbrightness = msg.payload.ledbrightness;
                dimmer.setLedBrightness(msg.payload.ledbrightness).catch(err => node.error(`while setting led bright: ${err.message}`));;
            }
        });

        node.on('close', () => {
            subscriptions.forEach(s => s.unsubscribe());
            dimmer.close();
            nodeLayer.close();
        });
    }

    RED.nodes.registerType('snode-dimmer', DimmerNode);
}