import { MessageLayer } from './interfaces';
import { Observable } from 'rxjs/Observable';

export interface MessageLayer {
    data: Observable<Buffer>;
    send(data: Buffer): Promise<void>;
}

export interface ConnectableMessageLayer extends MessageLayer {
    close();
    open(): Promise<void>;
}