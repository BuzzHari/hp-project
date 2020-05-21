import numpy as np
import matplotlib.pyplot as plt
data = np.array([[118310, 220586, 442268, 882996],
                 [41216, 81809, 163172, 324308],
                 [40857, 81599, 162892, 326593]])


x_labels = ['8MB', '16MB', '32MB', '64MB']
length = 4
print(length)
#X = np.asarray(x)

fig, ax = plt.subplots()
width = 0.2
x = np.arange(length)

ax.bar(x, data[0], width, color = 'skyblue', label='Serial')
ax.bar(x+width, data[1], width, color = 'orange', label='OpenCL')
ax.bar(x+width+width, data[2], width, color = 'green', label='CUDA') ##

ax.set_ylabel('Time in microseconds')
ax.set_ylim(0,930000)

ax.set_xticks(x + width/2)
ax.set_xticklabels(x_labels)
ax.set_xlabel('Data size in Bytes')
ax.set_title('Serial RLE vs OpenCL RLE')
ax.legend()

fig.tight_layout()
plt.show()
