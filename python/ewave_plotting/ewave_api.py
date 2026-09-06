import matplotlib.pyplot as plt
from flask import Flask, abort, redirect, render_template
from flask_bootstrap import Bootstrap
from flask_restful import reqparse, Api, Resource
from obspy.clients.earthworm import Client
from obspy.core.utcdatetime import UTCDateTime
from forms import DataRequestForm

import os
import socket

import settings
from logging import error  # , debug

settings.setup(__file__)

WAVESERVER_HOST = settings.get('waveserverHost')
WAVESERVER_PORT = settings.get('waveserverPort')
WIDTH = settings.get('width')
HEIGHT = settings.get('height')
DPI = settings.get('dpi')
FLASK_PORT = settings.get('ewavePort')
FLASK_HOST = settings.get('ewaveHost')

app = Flask(__name__, static_url_path='/static/')
app.config['SECRET_KEY'] = 'The curfew tolls the knell of the parting day'
bootstrap = Bootstrap(app)
api = Api(app)

parser = reqparse.RequestParser()
parser.add_argument('net', required=True, help="Network cannot be blank!")
parser.add_argument('sta', required=True, help="Station cannot be blank!")
parser.add_argument('cha', required=True, help="Channel cannot be blank!")
parser.add_argument('loc', required=True, help="Location cannot be blank;\
                    use -- for no location code!")
parser.add_argument('start', required=True, help="Start time cannot be blank!")
parser.add_argument('dur', required=True, type=int,
                    help="Duration must be non-zero integer value")
parser.add_argument('ptime', required=False)
parser.add_argument('plabel', required=False)
parser.add_argument('outputFormat', required=False)
parser.add_argument('displayMaxValue', required=False)
parser.add_argument('scaleFactor', required=False)
parser.add_argument('units', required=False)


@app.route('/ewave/form', methods=['GET', 'POST'])
def processForm():
    form = DataRequestForm()
    if form.validate_on_submit():
        net = form.net.data
        sta = form.sta.data
        loc = form.loc.data
        chan = form.chan.data
        start = form.start.data
        dur = form.dur.data
        picktime = form.picktime.data
        picklabel = form.picklabel.data
        outputFormat = form.outputFormat.data
        displayMaxValue = form.displayMax.data
        scaleFactor = form.scaleFactor.data
        units = form.units.data
        return redirect(
            '/ewave/query?net={}&sta={}&cha={}&loc={}&start={}\
                &dur={}&ptime={}&plabel={}&outputFormat={}&displayMaxValue={}\
                &scaleFactor={}&units={}'
            .format(net, sta, chan, loc, start, dur, picktime, picklabel,
                    outputFormat, displayMaxValue, scaleFactor, units))

    return render_template('form.html', form=form, net='', sta='', loc='',
                           chan='', start='', dur='', picktime='',
                           picklabel='', outputFormat='',
                           displayMaxValue=False, scaleFactor='', units='')


class GetRESTData(Resource):
    def get(self, request_string):
        host = WAVESERVER_HOST
        port = WAVESERVER_PORT
        hosts = []
        ports = []

        if isinstance(host, list):
            # check to see if num of hosts is num of ports
            if not isinstance(port, list):
                hosts.append(host[0])
                ports.append(port)
            elif (len(host) != len(port)):
                # just use the first item in each list:
                hosts.append(host[0])
                ports.append(port[0])
            else:
                for hst in host:
                    hosts.append(hst)
                for prt in port:
                    ports.append(prt)
        elif isinstance(port, list):
            # in this case, a single host is given with multiple ports
            # loop over given ports and check single host at all ports
            for prt in port:
                hosts.append(host)
                ports.append(prt)
        else:
            # here, we believe host and port are both single strings:
            hosts.append(host)
            ports.append(port)

        print('Configured host: {} conf. port: {}'.format(host, port))
        print('Using hosts: {} ports: {}'.format(hosts, ports))

        args = parser.parse_args()
        network = args['net']
        station = args['sta']
        location = args['loc']
        channel = args['cha']
        starttime = UTCDateTime(args['start'])
        endtime = UTCDateTime(starttime + int(args['dur']))
        ptime = args['ptime']
        plabel = args['plabel']
        outputFormat = args['outputFormat']
        displayMax = args['displayMaxValue']
        scaleFactor = args['scaleFactor']
        outputUnits = args['units']
        if displayMax:
            if scaleFactor is None:
                scaleFactor = '1.0'
            if outputUnits is None:
                outputUnits = 'counts'
        if outputFormat:
            if outputFormat == 'plot':
                outputFormat = 'png'
            if outputFormat == 'miniseed':
                outputFormat = 'mseed'
            if (outputFormat != 'png') and (outputFormat != 'mseed'):
                # these are the only supported output types at this time!
                abort(404)
        else:
            outputFormat = 'png'  # default

        # First, check the static/ directory to see if we have already acquired
        # this data before:
        outputFileName = '{}.{}.{}.{}_{}-{}.{}'.format(network, station,
                                                       location, channel,
                                                       starttime, endtime,
                                                       outputFormat)
        if os.path.exists('static/{}'.format(outputFileName)):
            return app.send_static_file(outputFileName)

        i = 0
        for hst in hosts:
            prt = ports[i]
            # check if we can make a connection to (addr, port) before
            # setting up client:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            errNo = s.connect_ex((hst, prt))
            if errNo is not 0:
                error('Error when attempting to connect: {}'
                      .format(os.strerror(errNo)))
                s.close()
                if (i == (len(hosts) - 1)):
                    abort(500)
                else:
                    i += 1
                    continue
            s.close()

            client = Client(hst, prt)
            if client.get_availability(network, station, location, channel):
                st = client.get_waveforms(network, station, location, channel,
                                          starttime, endtime)
                break
            else:
                i += 1
                if i == len(hosts):
                    # this means we've exhausted the list of waveservers
                    # without finding the data, abort
                    error('Unable to find data for {}.{}.{}.{} on any of \
                          of provided waveservers'.format(network, station,
                                                          location, channel))
                    abort(404)
                continue

        if st.count() == 0:
            abort(404)
        st.merge()
        if outputFormat == 'mseed':
            # write this out
            outFile = 'static/{}'.format(outputFileName)
            st.write(outFile, format="MSEED")
        else:
            self.plotData(st, outputFileName, ptime, plabel, displayMax,
                          scaleFactor, outputUnits)
        return app.send_static_file(outputFileName)

    def plotData(self, obspyStream, outputFileName, picktime,
                 picklabel, displayMaxValue, scaleFactor, units):
        if picktime:
            if picktime is '':
                picktime = None
            if picktime and not picklabel:
                picklabel = 'Pick'
        if displayMaxValue:
            if (displayMaxValue == '') or (displayMaxValue == 'False'):
                displayMaxValue = None
            if (scaleFactor is None) or (scaleFactor == ''):
                scaleFactor = '1.0'
            else:
                if float(scaleFactor) <= 0.0:
                    scaleFactor = '1.0'

        if (picktime is None) and (displayMaxValue is None):
            obspyStream.plot(outfile='static/{}'.format(outputFileName))
        else:
            tr = obspyStream[0]
            tr.detrend(type='demean')

            fig = plt.figure(num=None, dpi=DPI, figsize=(float(WIDTH) / DPI,
                                                         float(HEIGHT) / DPI))
            fig.set_dpi(DPI)
            fig.set_figwidth(float(WIDTH) / DPI)
            fig.set_figheight(float(HEIGHT) / DPI)
            ax = fig.add_subplot(1, 1, 1)
            fig.subplots_adjust(left=0.12, right=0.95, top=0.95,
                                bottom=0.05)
            ax.plot(tr.times("matplotlib"), tr.data, "k-", linewidth=0.5,
                    markeredgewidth=0.5)
            ax.xaxis_date()
            fig.autofmt_xdate(bottom=0.2, rotation=0, ha='right', which=None)
            bottom, top = ax.get_ylim()
            # print('left: {} right: {} difference: {}'.format(left, right,
            #                                                  (right - left)))
            # calculate fraction of time into wave on which to plot pick:
            start = tr.stats.starttime.matplotlib_date
            end = tr.stats.endtime.matplotlib_date
            # print('start: {} end: {}'.format(start, end))
            bbox_props = dict(boxstyle="round,pad=0.1", fc="0.8")
            if picktime:
                ptime = UTCDateTime(picktime).matplotlib_date
                yPos = 0
                ax.plot(ptime, yPos, "|", color="red", markeredgewidth=2,
                        markersize=150)
                yOffset = abs(top - bottom) * .1
                ax.annotate(picklabel, xy=(ptime, yPos),
                            xytext=(ptime, bottom + yOffset), fontsize='large',
                            fontweight='bold', bbox=bbox_props)
            if displayMaxValue:
                # get indices of max values:
                mxVal = tr.data.max()
                mnVal = tr.data.min()
                if abs(mnVal) > abs(mxVal):
                    mxVal = mnVal
                    mxIndex = tr.data.argmin()
                else:
                    mxIndex = tr.data.argmax()
                print('maximum value: {} max index: {}'.
                      format(mxVal, mxIndex))
                mxFrac = mxIndex/(len(tr))
                mxPos = mxFrac * (end - start)
                mxPos += start
                # print('mxFrac: {} mxPos: {}'.format(mxFrac, mxPos))
                scaledVal = float(mxVal) * float(scaleFactor)
                mxLabel = 'max = %.4f %s' % (float(scaledVal), units)
                arrowprops = dict(
                    arrowstyle="->", color="red",
                    connectionstyle="angle,angleA=0,angleB=90,rad=10")
                if (end - mxPos) > (mxPos - start):
                    # plot on the right side:
                    xTextPos = (end - mxPos) * 0.4 + mxPos
                else:
                    xTextPos = (mxPos - start) * 0.4 + start
                yTextPos = (top - mxVal)/4 + mxVal
                # print('xText: {} top: {} yText: {}'.
                #      format(xTextPos, top, yTextPos))
                ax.annotate(mxLabel, (mxPos, mxVal),
                            xytext=(xTextPos, yTextPos),
                            bbox=bbox_props, arrowprops=arrowprops)
            ax.set_xlabel('Time')
            ax.set_ylabel('Counts')
            trace_label = '{}.{}.{}.{}'.format(tr.stats.network,
                                               tr.stats.station,
                                               tr.stats.location,
                                               tr.stats.channel)
            ax.text(0.02, 0.95, trace_label, transform=ax.transAxes,
                    fontdict=dict(fontsize="small", ha='left', va='top'),
                    bbox=dict(boxstyle="round", fc="w", alpha=0.8))
            # save the plot:
            fig.savefig(fname='static/{}'.format(outputFileName))
            plt.close(fig)


api.add_resource(GetRESTData, '/ewave/<request_string>')


if __name__ == '__main__':
    app.run(debug=False, host=FLASK_HOST, port=FLASK_PORT)
