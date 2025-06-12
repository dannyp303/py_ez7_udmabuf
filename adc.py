import time
import argparse

from pyzmodlib import ZMODADC1410

TRANSFER_LEN = 0x400
IIC_BASE_ADDR = 0xE0005000
ZMOD_IRQ = [63, 61]

# Base addresses for each ADC instance
ADC_BASE_ADDRS = [0x43C00000, 0x40000000]
ADC_DMA_BASE_ADDRS = [0x40400000, 0x40410000]
ADC_FLASH_ADDR = 0x30
ADC_DMA_IRQS = [64, 62]  # Replace 63 with correct IRQ if different


def writeADCData(adcZmod, filename, acqBuffer, channel, gain, length):
    #print("Entering function: writeADCData", flush=True)
    ret = []
    with open(filename, 'w') as f:
        #print("Channel, Time", file=f, flush=True)

        for i in range(length):
            time_formatted = f"{i * 10}ns" if i < 100 else f"{i / 100.0:.2f}us"
            data = adcZmod.get_data(acqBuffer, i)
            valCh = adcZmod.signed_channel_data(channel, data)
            val = adcZmod.get_volt_from_signed_raw(valCh, gain)
            val_formatted = f"{1000.0 * val:.2f}mV"
            #print(f"{time_formatted}, {val_formatted}", file=f, flush=True)
            #print(f"{time_formatted}, {val_formatted}", flush=True)
            ret.append(val)
    return ret

def adcDemo(adcZmod, channel, gain, length, repetition=1, trigger=False):
    #print("Entering function: adcDemo", flush=True)
    acqBuffer = 0
    adcZmod.set_gain(channel, gain)
    ret = []
    for i in range(repetition):
        #print("Allocating channels buffer...", flush=True)
        acqBuffer = adcZmod.alloc_channels_buffer(length)
        #print("Buffer allocated.", flush=True)
        
        #print("Starting acquisition...", flush=True)
        if trigger:
            res = adcZmod.acquire_triggered_polling(acqBuffer, channel, 0, 0, 0, length)
        else:
            res = adcZmod.acquire_immediate_polling(acqBuffer, length)
        if res != 0:
            print("Acquisition failed")
            return ret
        #print("Acquisition complete.", flush=True)
        
        #print("Writing ADC data to file...", flush=True)
        dat = writeADCData(adcZmod, "buffer_data.csv", acqBuffer, channel, gain, length)
        #print("Data written to file.", flush=True)
        # Print results in mV

        #print("Freeing channels buffer...", flush=True)
        adcZmod.free_channels_buffer(acqBuffer, length)
        time.sleep(2)
        print("ADC Data (in mV):", flush=True)
        for value in dat:
            print(f"{value * 1000:.2f} mV")
        #print("Woke up from sleep.", flush=True)
    return ret



def main():
    #print("Entering function: main", flush=True)
    parser = argparse.ArgumentParser(description="ZmodADC1410 Demo")
    parser.add_argument("--adc", type=int, choices=[0, 1], default=0, help="Select ADC index: 0 or 1")
    parser.add_argument("--channel", type=int, choices=[0, 1], default=0, help="Select ADC channel: 0 or 1")
    parser.add_argument("--gain", type=int, default=0, help="Set gain (0=low, 1=high)")
    parser.add_argument("--length", type=int, default=TRANSFER_LEN, help="Acquisition length")
    parser.add_argument("--repetition", type=int, default=1, help="Number of repeated acquisitions")
    parser.add_argument("--trigger", action="store_true", default=False, help="Trigger mode")
    parser.add_argument("--ramp", action="store_true", default=False, help="Ramp mode")
    args = parser.parse_args()

    adc_index = args.adc
    #print(f"Creating ADC {adc_index} on channel {args.channel} with gain {args.gain}", flush=True)
    adcZmod = ZMODADC1410(
        ADC_BASE_ADDRS[adc_index],
        ADC_DMA_BASE_ADDRS[adc_index],
        IIC_BASE_ADDR,
        ADC_FLASH_ADDR,
        ZMOD_IRQ[adc_index],
        ADC_DMA_IRQS[adc_index]
    )

    #print(f"Starting ADC {adc_index} on channel {args.channel} with gain {args.gain}", flush=True)
    if args.ramp:
        dat = adcZmod.auto_test_ramp(args.channel, 0, 0, 0, args.length)
        if dat != 0:
            print("Ramp failed", flush=True)
            return
    else:
        dat = adcDemo(adcZmod, args.channel, args.gain, args.length, args.repetition, args.trigger)
    #print('Read data done', flush=True)
    #print(dat, flush=True)



if __name__ == "__main__":
    start = time.time()
    main()
    end = time.time()
    print(f"Time taken: {end - start} seconds")

