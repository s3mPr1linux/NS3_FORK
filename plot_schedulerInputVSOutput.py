#!/usr/bin/python3
import matplotlib.pyplot as plt
import numpy

def plot_scheduler_input_n_output(standalone_plot=False, ax1=None):
    bufferFileIn = ""
    bufferFileOut = ""

    with open("./build/bin/schedulerInput.txt",'r') as file:
        bufferFileIn = file.readlines()

    with open("./build/bin/schedulerOutput.txt",'r') as file:
        bufferFileOut = file.readlines()

    plt.ioff()

    #Discard first lines
    bufferFileIn = bufferFileIn[1:]
    bufferFileOut = bufferFileOut[1:]

    bufferFileIn = bufferFileIn[:-3]
    bufferFileOut = bufferFileOut[:-3]


    inputAndOutputBitmapPerInterval = {}

    for intervalBitmap in bufferFileIn:
        y = intervalBitmap.split(sep=':')

        interval = y[0] #"+350000000.0ns"
        interval = interval.split(sep=".")#["+350000000", "0ns"]
        interval = interval[0]#"+350000000"
        interval = int(interval)#350000000
        interval /= 1000000 #350

        inputAndOutputBitmapPerInterval[interval] = [int(y[1],2), 0]

    for intervalBitmap in bufferFileOut:
        y = intervalBitmap.split(sep=':')

        interval = y[0] #"+350000000.0ns"
        interval = interval.split(sep=".")#["+350000000", "0ns"]
        interval = interval[0]#"+350000000"
        interval = int(interval)#350000000
        interval /= 1000000 #350

        if interval not in inputAndOutputBitmapPerInterval:
            inputAndOutputBitmapPerInterval[interval] = [0, int(y[1],2)]
        else:
            inputAndOutputBitmapPerInterval[interval][1] = int(y[1],2)

    ax = None
    if standalone_plot:
        fig, ax = plt.subplots()
    else:
        if ax1 is None:
            exit(-1)
        else:
            ax = ax1

    sortedInputOutputBitmapKeys = list(sorted(inputAndOutputBitmapPerInterval.keys()))

    ax.set_xlabel('time (ms)', )
    ax.set_ylabel('RBG #')
    #ax.grid(True)
    ax.tick_params('y')
    yticks = [x for x in list(range(0, 25, 4))]
    xticks = [float(x) for x in range(5000)]
    for i in range(25):

        input_barh = []
        output_barh = []

        for interval in sortedInputOutputBitmapKeys:

            bitmaps = inputAndOutputBitmapPerInterval[interval]
            marked = False
            if ( (bitmaps[0]>>i) & 0x01):
                    input_barh += [(float(interval),1.0)] #Blocks occupied before scheduling (unexpected access reported by UEs, may be a PU,SU,whatever)
                    marked = True
            if ( (bitmaps[1]>>i) & 0x01) and not marked:
                    output_barh += [(float(interval),1.0)] #Blocks occupied by transmissions scheduled by the eNB

        ax.broken_barh(input_barh,(i,1.0), facecolors="red", alpha=0.7) #Blocks occupied before scheduling (unexpected access reported by UEs, may be a PU,SU,whatever)
        ax.broken_barh(output_barh,(i,1.0), facecolors="blue", alpha=0.5) #Blocks occupied before scheduling (unexpected access reported by UEs, may be a PU,SU,whatever)
        ax.set_yticks(yticks)
        #ax.set_xticks(xticks)
        plt.show(block=False)
        pass
    pass


if __name__ == "__main__":
    plot_scheduler_input_n_output(standalone_plot=True)
    input()
    pass