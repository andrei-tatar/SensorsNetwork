
import * as winston from 'winston';
export let logger = new winston.Logger();
logger.add(winston.transports.Console, { level: 'silly', timestamp: true, colorize: true });