/*
 * This sets three bits for each channel, one channel at a time.
 * For channel 0 we want to set bits 1, 2 & 3
 * For channel 1 we want to set bits 4, 5 & 6
 *
 * We convert the input values that are strange due to original
 * firmware code into the value of the three bits as follows:
 *
 * val -> bits
 * 1  -> 010b
 * 2  -> 001b
 * 5  -> 000b
 * 10 -> 011b
 *
 * The third bit is always zero since there are only four outputs connected
 * in the serial selector chip.
 *
 * The multiplication of the converted value by 0x24 sets the relevant bits in
 * both channels and then we mask it out to only affect the channel currently
 * requested.
 */

static BOOL set_voltage(BYTE channel, BYTE val)
{
	BYTE bits, mask;

	switch (val) {
	case 1:
		bits = 0x02;
		break;
	case 2:
		bits = 0x01;
		break;
	case 5:
		bits = 0x00;
		break;
	case 10:
		bits = 0x03;
		break;
	default:
		return FALSE;
	}

	bits = bits << (channel ? 1 : 4);
	mask = (channel) ? 0x70 : 0x0e;
	IOA = (IOA & ~mask) | (bits & mask);

	return TRUE;
}