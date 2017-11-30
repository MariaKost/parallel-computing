import matplotlib.pyplot as plt
import numpy as np
import sys

lines = [line for line in sys.stdin]
stats_log = lines[-1].split(' ')
population_size = stats_log[1]
num_of_verteces = stats_log[3]
results = np.array([[float(word) for word in line.split(' ')] for line in lines[:-1]])

plt.figure(figsize=(12, 7))
plt.title("Number of verteces: {0}, Population size: {1}".format(num_of_verteces, population_size))
plt.plot(results[:, 0], results[:, 1], label = "Best route")
plt.plot(results[:, 0], results[:, 2], label = "Worst route")
plt.plot(results[:, 0], results[:, 3], label = "AVG")
plt.xlim(0)
plt.ylim(0)
plt.xlabel("Iteration number")
plt.ylabel("Route length")
plt.legend()
plt.savefig("N={0}, Population={1}.png".format(num_of_verteces, population_size))
