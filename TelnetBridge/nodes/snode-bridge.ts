module.exports = function (RED) {

    function BridgeNode(config) {
        RED.nodes.createNode(this, config);
        const node = this;

        console.log('created bridge node!');
    }

    RED.nodes.registerType('snode-bridge', BridgeNode);
}