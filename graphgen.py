#!/usr/bin/python3
import csv
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import matplotlib.font_manager as fm
import matplotlib.lines as mlines

fontPath = "/usr/share/fonts/truetype/crosextra/Carlito-Regular.ttf"
headerSize = 26
fontSize = 14

def loadData():
	y_axis = []
	x_axis = []

	with open('sort_times_') as file:
		for row in file:
			srow = row.split()
			x_axis.append(int(srow[0]))
			y_axis.append(float(srow[1]))
	return x_axis,y_axis


def main():
	x_axis,y_axis = loadData()
	exportGraph(x_axis, y_axis)
	
	#print(np.diagonal(table[:,0,:],-7))


'''
x_axis 		array of values on x axis
y_axis 		parsed table from previous funcion
'''
def exportGraph(x_axis,y_axis):
	fig = plt.figure(figsize=(6, 4))
	plot = fig.add_subplot(111)

	font= fm.FontProperties(fname=fontPath)

	#plot.set_title(u'Silné škálovanie', fontproperties=font, fontsize = headerSize)
	plot.set_ylabel(u'Čas [s]', fontproperties=font, fontsize=fontSize)
	plot.set_xlabel(u'Dĺžka radenej postupnosti [B]', fontproperties=font, fontsize=fontSize)

	plot.plot(x_axis, y_axis, linestyle='-', marker='', color='blue', linewidth=1.)
	plot.set_xlim(0,1000000)
	#plot.loglog(x_axis, table[domainIdx,idx], basex=2, linestyle='-', marker=marker, color=color)
	#plt.grid()

	ax = plt.gca()
	ax.grid(b=True, which='major', color='k', linestyle='-', alpha=0.3)

	fig.tight_layout()
	#plt.show()
	plt.savefig("graph.png")
	fig.clf()


if __name__ == '__main__':
	main()
