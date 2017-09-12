import { MessageLayer } from './interfaces';
import { Observable } from 'rxjs/Observable';

export interface MessageLayer {
    data: Observable<Buffer>;
    send(data: Buffer): Promise<void>;
}

export interface ConnectableMessageLayer extends MessageLayer {
    close();
}

export interface Logger {
    log(...args: any[]);
    info(...args: any[]);
    warn(...args: any[]);
    error(...args: any[]);

    trace(...args: any[]);
    debug(...args: any[]);
}

export interface NodeRedNode {
    error(msg: string);
    status(options: { fill: string, shape: string, text: string });
    send(msg: { payload });
    on(event: 'input' | 'close', callback: (msg) => void);
}