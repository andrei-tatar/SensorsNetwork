import { logger } from './logger';
import { Communication } from './communication/communication';
import { PackageCommunication } from './communication/packagecommunication';
import { Telnet } from './communication/telnet';

import { Dimmer } from './dimmer';
import { Node } from './node';
import * as _ from 'lodash';

function delay(time: number) {
    return new Promise(resolve => setTimeout(resolve, time));
}

async function start() {
    try {
        var telnetLayer = new Telnet('192.168.1.50', 23);
        await telnetLayer.open();

        var packageLayer = new PackageCommunication(telnetLayer);
        var comm = new Communication(packageLayer);
        


    } catch (e) {
        logger.error(e.message);
    }

}

start();