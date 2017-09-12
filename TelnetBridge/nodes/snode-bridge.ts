import { Communication } from './../communication/communication';
import { PackageCommunication } from './../communication/packagecommunication';
import { Telnet } from './../communication/telnet';
module.exports = function (RED) {

    function BridgeNode(config) {
        RED.nodes.createNode(this, config);
        const node = this;

        const telnetLayer = new Telnet(config.host, config.port, RED.log);
        const packageLayer = new PackageCommunication(telnetLayer);
        const communicationLayer = new Communication(packageLayer);
        node.communication = communicationLayer;
        node.connected = telnetLayer.connected;

        const subscription = telnetLayer.connected.subscribe(async connected => {
            if (connected) {
                try {
                    await communicationLayer.init(true);
                } catch (err) {
                    node.error(`initializing communication`, err);
                }
            }
        });

        telnetLayer.open();
        node.on("close", () => {
            subscription.unsubscribe();
            communicationLayer.close();
            telnetLayer.close();
        });
    }

    RED.nodes.registerType('snode-bridge', BridgeNode);
}