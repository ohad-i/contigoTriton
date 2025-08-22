import PySimpleGUI as sg
import time
import socket
from select import select
import struct
import time
import pickle
import config


stopMovingOp   = 0x0801
startMovingOp  = 0x0802
doRotationsOp  = 0x0803
gotoPositionOP = 0x0804
setSpeedOp     = 0x0805
setServoOp     = 0x0806
burnServoIdOp  = 0x0807
messengerOp    = 0x0808;


def led_indicator(key=None, radius=30):
    indicator = sg.Graph(canvas_size=(radius, radius), 
                         graph_bottom_left=(-radius, -radius), 
                         graph_top_right=(radius, radius), 
                         pad=(0, 0), 
                         key=key)
    return indicator


def set_led(window, key, color):
    graph = window[key]
    graph.erase()
    graph.draw_circle((0, 0), 20, fill_color=color, line_color=color)

def build_gui():
    sg.theme('DarkAmber')

    # Define the core layout for a single tab
    # We'll create a function or a variable to hold this to avoid repetition
    tab2_layout = [
            [sg.Text('cmdPort:', size=(9, 1)), sg.InputText(size=(15, 1), key=f'_cmdPort_', justification='center', default_text='9105', disabled=True)],
            [sg.Text('metryPort:', size=(9, 1)), sg.InputText(size=(15, 1), key=f'_metryPort_', justification='center', default_text='9106', disabled=True)],
            [sg.Text('StationIp:', size=(9, 1)), sg.InputText(size=(15, 1), key=f'_stationIp_', justification='center', default_text='127.0.0.1')],
            [sg.Text('servoId:', size=(9, 1)), sg.InputText(size=(15, 1), key=f'_servoInputId_', justification='center', default_text='22'), sg.Button('Burn servo ID', size=(13, 0), key=f'_burnId_')],
            [led_indicator(f'_connected_'), sg.Text('Connected', size=(20, 1))],
            [led_indicator(f'_isMoving_'), sg.Text('Is moving', size=(20, 1))],
            [sg.Text('Position value [deg]: ---', key=f'_position_', size=(20, 1)), sg.Text('Speed: ---', key=f'_speed_')],
            [sg.Text('Load: ---', key=f'_load_', size=(20, 1)), sg.Text('Volt[V]: ---', key=f'_volt_')],
            [sg.Text('Temp[c]: ---', key=f'_temp_', size=(20, 1)), sg.Text('Servo ID: ---', key=f'_servoId_')],
            [sg.Text('Current[A]: ---', key=f'_current_', size=(20, 1)), sg.Text('Rotations: ---', key=f'_rots_')],
            [sg.Button('Set servo', size=(13, 0), key=f'_setServo_')],
            [sg.Button('Start moving', size=(13, 0), key=f'_startMoving_'), sg.Text('speed [-2200 - 2200]:'), sg.InputText(size=(5, 1), key=f'_wheelSpeed_', justification='center', default_text='500')],
            [sg.Button('Change speed', size=(13, 0), key=f'_changeSpeed_'), sg.Text('speed [-2200 - 2200]:'), sg.InputText(size=(5, 1), key=f'_newSpeed_', justification='center', default_text='500')],
            [sg.Button('Stop moving', size=(13, 0), key=f'_stopMoving_')],
            [sg.Button('Cycles', size=(13, 0), key=f'_cycles_'), sg.Text('Num cycles:', size=(14, 1)), sg.InputText(size=(5, 1), key=f'_numCycles_', justification='center', default_text='1'), sg.Text('Speed [-2200 - 2200]:'), sg.InputText(size=(5, 1), key=f'_cyclesSpeed_', justification='center', default_text='500')],
            [sg.Button('set Servo position', size=(13, 0), key=f'_setServoPos_'), sg.Text('Pos [deg] [0-360]:'), sg.InputText(size=(5, 1), key=f'_posDeg_', justification='center', default_text='0'), sg.Text('Speed [1-2200, 0->2200]]:'), sg.InputText(size=(5, 1), key=f'_posSpeed_', justification='center', default_text='500')],
            ]
        

    # Create the layouts for each tab using the helper function
    tab1_layout = [
            [led_indicator(f'_connected_main_'), sg.Text('Connected', size=(20, 1))],
            [led_indicator(f'_isMoving_main_'), sg.Text('Is moving', size=(20, 1))],
            [led_indicator(f'_load_main_'), sg.Text('Oveloaded warning', size=(20, 1))],
            [   sg.Text('Length [m]:', size=(14, 1)),
                sg.InputText(size=(5, 1), key=f'_length_m_', justification='center', default_text='1'), 
                sg.Text('Speed [0 - 2200]:'), sg.InputText(size=(5, 1), key=f'_wireSpeed_', justification='center', default_text='1000'),
                sg.Button('Collect', size=(13, 0), key=f'_collect_'),
                sg.Button('Release', size=(13, 0), key=f'_release_'),
                ],
            [sg.Button('Stop', size=(13, 0), key=f'_stopMoving_main_'), sg.Button('Collect all', size=(13, 0), key=f'_collect_all_')],
            [sg.Button('Open messenger', size=(13, 0), key=f'_open_messenger_'), sg.Button('Close messenger', size=(13, 0), key=f'_close_messenger_') ]
                ]

    # Create the tab group
    tab_group_layout = [
        [sg.Tab('Main Control', tab1_layout, key='_TAB1_')],
        [sg.Tab('Setup', tab2_layout, key='_TAB2_')]
    ]

    # Main window layout with the tab group
    main_layout = [[sg.TabGroup(tab_group_layout, enable_events=True, key='_TAB_GROUP_')]]

    window = sg.Window('Servo Control Application', main_layout, font=('Arial', 15))

    window.finalize()

    # Initialize LED indicators for both tabs
    set_led(window, '_connected_', 'red')
    set_led(window, '_isMoving_', 'red')
    set_led(window, '_connected_main_', 'red')
    set_led(window, '_isMoving_main_', 'red')
    set_led(window, '_load_main_', 'orange') #_load_main_  
    
    return window


def run_gui(window):

    event, values = window.read(timeout=10)

    sock = socket.socket(socket.AF_INET, # Internet
        socket.SOCK_DGRAM) # UDP
    
    sock.bind(("", int(values['_metryPort_'])))

    hostIpPort = (values['_stationIp_'], int(values['_cmdPort_']) )
    isLastIsMoving = time.time()

    while True:
            event, values = window.read(timeout=0.001)

            if time.time() - isLastIsMoving > 3:
                set_led(window, '_isMoving_', 'orange')
                set_led(window, '_isMoving_main_', 'orange')
                set_led(window, '_connected_', 'red')
                set_led(window, '_connected_main_', 'red')

                isLastIsMoving = time.time()

            socks = select([sock], [], [], 0.01)[0]
            
            for _ in range(len(socks)):
                data, addr = socks[0].recvfrom(64*1024)
                data = pickle.loads(data)
                if data[0] ==  b'servoMetry':
                    window['_position_'].update("Position value: %0.2f"%data[1]['pos'])
                    window['_speed_'].update("Speed: %d"%data[1]['speed'])
                    window['_load_'].update("Load: %d"%data[1]['load'])
                    
                    if data[1]['move'] == 1:
                        print( (data[1]['load']),  data[1]['current'])
                        if abs(data[1]['load']) > config.loadWarningThr and data[1]['current'] > config.ampWarningThr:
                            set_led(window, '_load_main_', 'red')
                        else:
                            set_led(window, '_load_main_', 'green')
                    else:
                        if abs(data[1]['load']) > config.staticLoadWarningThr:
                            set_led(window, '_load_main_', 'red')
                        else:
                            set_led(window, '_load_main_', 'green')

                    window['_volt_'].update("Volt[V]: %0.2f"%data[1]['volt'])
                    window['_temp_'].update("Temp[C]: %d"%data[1]['temper'])
                    window['_servoId_'].update("Servo ID: %d"%data[1]['id'])
                    window['_current_'].update("Current[A]: %0.2f"%data[1]['current'])
                    window['_rots_'].update("Rotations: %d"%data[1]['rots'])
                    if data[1]['move'] == 1:
                        set_led(window, '_isMoving_', 'green')
                        set_led(window, '_connected_', 'green')
                        set_led(window, '_isMoving_main_', 'green')
                        set_led(window, '_connected_main_', 'green')
                        isLastIsMoving = time.time()

                    elif  data[1]['move'] == 0:
                        set_led(window, '_isMoving_', 'red')
                        set_led(window, '_connected_', 'green')
                        set_led(window, '_isMoving_main_', 'red')
                        set_led(window, '_connected_main_', 'green')
                        isLastIsMoving = time.time()


            
            if event == sg.WIN_CLOSED:	# if user closes window or clicks cancel
                break

            elif event == '_collect_':
                
                length = float(values['_length_m_'])

                numCycles = length/config.wheelCircum
                print('collect numCycles', numCycles)
                numCycles = int(numCycles*10)
                cycleSpeed = int(values['_wireSpeed_'])*config.collectDirection
                servoId = int(values['_servoInputId_'])
    
                msg = [doRotationsOp, servoId, numCycles, cycleSpeed, 0, 0]
                stationIpPort = (values['_stationIp_'], int(values['_cmdPort_']) )
                sock.sendto(pickle.dumps(msg), stationIpPort)

            elif event == '_release_':
                
                length = float(values['_length_m_'])

                numCycles = length/config.wheelCircum
                print('release numCycles', numCycles)
                numCycles = int(numCycles*10)
                cycleSpeed = int(values['_wireSpeed_'])*(config.collectDirection*-1)
                servoId = int(values['_servoInputId_'])
    
                msg = [doRotationsOp, servoId, numCycles, cycleSpeed, 0, 0]
                stationIpPort = (values['_stationIp_'], int(values['_cmdPort_']) )
                sock.sendto(pickle.dumps(msg), stationIpPort)

            elif event == '_burnId_':
                servoId = int(values['_servoInputId_'])
                msg = [burnServoIdOp, servoId, 0, 0, 0, 0]
                stationIpPort = (values['_stationIp_'], int(values['_cmdPort_']) )
                sock.sendto(pickle.dumps(msg), stationIpPort)


            elif event == '_setServo_':
                servoId = int(values['_servoInputId_'])
                msg = [setServoOp, servoId, 0, 0, 0, 0]
                stationIpPort = (values['_stationIp_'], int(values['_cmdPort_']) )
                sock.sendto(pickle.dumps(msg), stationIpPort)

            elif event == '_startMoving_':
                speed = int(values['_wheelSpeed_'])
                servoId = int(values['_servoInputId_'])
                msg = [startMovingOp, servoId, speed, 0, 0, 0]
                stationIpPort = (values['_stationIp_'], int(values['_cmdPort_']) )
                sock.sendto(pickle.dumps(msg), stationIpPort)

            elif event == '_collect_all_':
                speed = 2200*config.collectDirection
                servoId = config.defaultServoId
                msg = [startMovingOp, servoId, speed, 0, 0, 0]
                stationIpPort = (values['_stationIp_'], int(values['_cmdPort_']) )
                sock.sendto(pickle.dumps(msg), stationIpPort)


            elif event == '_changeSpeed_':
                speed = int(values['_newSpeed_'])
                servoId = int(values['_servoInputId_'])
                msg = [setSpeedOp, servoId, speed, 0, 0, 0]
                stationIpPort = (values['_stationIp_'], int(values['_cmdPort_']) )
                sock.sendto(pickle.dumps(msg), stationIpPort)

            elif event == '_stopMoving_' or event == '_stopMoving_main_':
                servoId = int(values['_servoInputId_'])
                msg = [stopMovingOp , servoId, 0, 0, 0, 0]
                stationIpPort = (values['_stationIp_'], int(values['_cmdPort_']) )
                sock.sendto(pickle.dumps(msg), stationIpPort)

            elif event == '_cycles_':
                numCycles = float(values['_numCycles_'])
                numCycles = int(numCycles*10)
                cycleSpeed = int(values['_cyclesSpeed_'])
                servoId = int(values['_servoInputId_'])

                msg = [doRotationsOp, servoId, numCycles, cycleSpeed, 0, 0]
                stationIpPort = (values['_stationIp_'], int(values['_cmdPort_']) )
                sock.sendto(pickle.dumps(msg), stationIpPort)

            elif event == '_open_messenger_':
                
                PWM = int(config.openMessPwm)
                msg = [messengerOp, PWM, 0, 0, 0, 0]
                stationIpPort = (values['_stationIp_'], int(values['_cmdPort_']) )
                sock.sendto(pickle.dumps(msg), stationIpPort)
 
            elif event == '_close_messenger_':
               
                PWM = int(config.closeMessPwm)
                msg = [messengerOp, PWM, 0, 0, 0, 0]
                stationIpPort = (values['_stationIp_'], int(values['_cmdPort_']) )
                sock.sendto(pickle.dumps(msg), stationIpPort)
                
            elif event == '_setServoPos_':
                #posEnc = int(int(values['_posDeg_'])*11.375)
                posEnc = int(values['_posDeg_'])
                cycleSpeed = int(values['_posSpeed_'])
                servoId = int(values['_servoInputId_'])

                msg = [gotoPositionOP, servoId, posEnc, cycleSpeed, 0, 0]
                stationIpPort = (values['_stationIp_'], int(values['_cmdPort_']) )
                sock.sendto(pickle.dumps(msg), stationIpPort)


                

    window.close()

run_gui(build_gui())
