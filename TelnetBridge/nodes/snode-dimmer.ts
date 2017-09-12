module.exports = function (RED) {

    function DimmerNode(config) {
        RED.nodes.createNode(this, config);
        const node = this;

        const bridge = RED.nodes.getNode(config.bridge);
        if (!bridge) return;

        console.log(bridge);

        node.on('close', () => {

        });
    }

    RED.nodes.registerType('snode-dimmer', DimmerNode);
}