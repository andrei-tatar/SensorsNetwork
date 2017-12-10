import { IDecoder } from './decoder';

export class NecDecoder implements IDecoder {
    private static readonly max_error = 21; //%

    private static readonly header_mark = 9000;
    private static readonly header_space = 4500;
    private static readonly repeat_space = 2250;

    private static readonly bit_mark = 562;
    private static readonly zero_space = 562;
    private static readonly one_space = 1687;

    private matches(value, target) {
        return Math.abs(value - target) / target * 100 < NecDecoder.max_error;
    }

    decode(pulses: number[]): string {
        if (pulses.length < 3)
            return null;

        if (!this.matches(pulses[0], NecDecoder.header_mark))
            return null;

        if (this.matches(pulses[1], NecDecoder.repeat_space)) {
            if (this.matches(pulses[2], NecDecoder.bit_mark) && pulses.length == 3)
                return "NEC_REPEAT";

            return null;
        }

        if (!this.matches(pulses[1], NecDecoder.header_space) || pulses.length != 67)
            return null;

        var message = 0;
        for (var i = 0; i < 32; i++) {
            message *= 2;

            if (!this.matches(pulses[2 + i * 2], NecDecoder.bit_mark))
                return null;

            if (this.matches(pulses[3 + i * 2], NecDecoder.zero_space))
                continue;

            if (this.matches(pulses[3 + i * 2], NecDecoder.one_space)) {
                message += 1;
                continue;
            }

            return null;
        }

        if (!this.matches(pulses[66], NecDecoder.bit_mark))
            return null;

        return 'NEC_' + message.toString(16);
    }

    encode(code: string): number[] {
        if (typeof code != "string")
            return null;

        var parts = code.split('_');
        if (parts.length != 2 || parts[0] != 'NEC')
            return null;

        if (parts[1] == 'REPEAT') {
            return [NecDecoder.header_mark, NecDecoder.repeat_space, NecDecoder.bit_mark];
        }

        var nr = parseInt(parts[1], 16);
        if (isNaN(nr)) return null;

        var pulses = [NecDecoder.header_mark, NecDecoder.header_space];
        for (var i = 31; i >= 0; i--) {
            pulses.push(NecDecoder.bit_mark);
            pulses.push((nr & (1 << i)) !== 0 ? NecDecoder.one_space : NecDecoder.zero_space);
        }
        pulses.push(NecDecoder.bit_mark);

        return pulses;
    }
}