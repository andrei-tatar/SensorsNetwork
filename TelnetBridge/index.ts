import { DummyTelenet } from './communication/dummytelnet';
import { Dimmer } from './sensors/dimmer';
import { logger } from './logger';
import { Communication } from './communication/communication';
import { PackageCommunication } from './communication/packagecommunication';
import { Telnet } from './communication/telnet';

import * as _ from 'lodash';

function delay(time: number) {
    return new Promise(resolve => setTimeout(resolve, time));
}

async function start() {
    try {
        // var telnetLayer = new Telnet('192.168.1.50', 23);
        var telnetLayer = new DummyTelenet();
        await telnetLayer.open();

        var packageLayer = new PackageCommunication(telnetLayer);
        var comm = new Communication(packageLayer);

        var msgLayer1 = comm.register(Buffer.from([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]));
        var msgLayer2 = comm.register(Buffer.from([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]));

        var dimmer1 = new Dimmer(msgLayer1);
        await dimmer1.requestStateUpdate();
        await dimmer1.setBrightness(100);


    } catch (e) {
        logger.error(e.message);
    }

}

start();