import sys
import statistics
throughputs = (sys.argv)[1:-1]
throughputs = [float(i) for i in throughputs]
print(statistics.mean(throughputs))