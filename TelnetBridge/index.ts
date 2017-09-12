import { Dimmer } from './dimmer';
import { Node } from './node';
import { Communication } from './communication';

function delay(time: number) {
    return new Promise(resolve => setTimeout(resolve, time));
}

async function start() {
    const host = '192.168.1.50';
    var comm = new Communication(host);

   
}

start();