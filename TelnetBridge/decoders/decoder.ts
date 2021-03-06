import { SamsungDecoder } from './samsung';
import { NecDecoder } from './nec';
import { SonyDecoder } from './sony';

export class Decoder implements IDecoder {
    
    private decoders: IDecoder[] = [];

    constructor() {
        this.decoders.push(new SonyDecoder());
        this.decoders.push(new NecDecoder());
        this.decoders.push(new SamsungDecoder());
    }

    decode(pulses: number[]): string {
        for (const decoder of this.decoders) {
            const code = decoder.decode(pulses);
            if (code) return code;
        }
        return null;
    }

    encode(code: string): number[] {
        for (const decoder of this.decoders) {
            const pulses = decoder.encode(code);
            if (pulses) return pulses;
        }
        return null;
    }
}

export interface IDecoder {
    decode(pulses: number[]): string;
    encode(code: string): number[];
}
