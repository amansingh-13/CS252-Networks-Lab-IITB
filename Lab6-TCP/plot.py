import sys
import math
import pandas as pd
import matplotlib.pyplot as plt

## 2.093 +- sigma/sqrt(20)

df = pd.read_csv("output.csv", header=0)

delay = [10, 50, 100]
loss = [0.1, 0.5, 1]
# Fixed loss
for i in range(3):
    get_df = df[df['Loss'] == loss[i]]
    means_reno = get_df[get_df['Variant'] == "reno"]['Mean']
    error_reno = 2.093*get_df[get_df['Variant'] == "reno"]['Stdev']/(math.sqrt(20))

    means_cubic = get_df[get_df['Variant'] == "cubic"]['Mean']
    error_cubic = 2.093*get_df[get_df['Variant'] == "cubic"]['Stdev']/(math.sqrt(20))

    fig = plt.figure()
    plt.errorbar(delay,
                 means_reno,
                 yerr=error_reno,
                 label='Reno with Loss = ' + str(loss[i]) + '%',
                 capsize=15)
    plt.errorbar(delay,
                 means_cubic,
                 yerr=error_cubic,
                 label='Cubic with Loss = ' + str(loss[i]) + '%',
                 capsize=15)
    plt.legend(loc='upper right')
    plt.xlabel("Delay(in ms)")
    plt.ylabel("Throughput(in bits/sec)")
    plt.title("Throughput vs Delay : Loss = " + str(loss[i]) + "%")
    plt.show()

# Fixed Delay
for i in range(3):
    get_df = df[df['Delay'] == delay[i]]
    means_reno = get_df[get_df['Variant'] == "reno"]['Mean']
    error_reno = 2.093*get_df[get_df['Variant'] == "reno"]['Stdev']/(math.sqrt(20))

    means_cubic = get_df[get_df['Variant'] == "cubic"]['Mean']
    error_cubic = 2.093*get_df[get_df['Variant'] == "cubic"]['Stdev']/(math.sqrt(20))

    fig = plt.figure()
    plt.errorbar(loss,
                 means_reno,
                 yerr=error_reno,
                 label='Reno with Delay = ' + str(delay[i]) + 'ms',
                 capsize=15)
    plt.errorbar(loss,
                 means_cubic,
                 yerr=error_cubic,
                 label='Cubic with Delay = ' + str(delay[i]) + 'ms',
                 capsize=15)
    plt.legend(loc='upper right')
    plt.xlabel("Loss in %")
    plt.ylabel("Throughput(in bits/sec)")
    plt.title("Throughput vs Loss : Delay = " + str(delay[i]) + "ms")
    plt.show()