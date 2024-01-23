import os
import time
import pandas as pd
import subprocess
import threading
import csv
import mqtt
from datetime import datetime as dt
from paho.mqtt.client import connack_string as ack


# Three functions for threads
def run_detection_script():
    global process
    command = "python"
    script = "yolov5/detect.py"
    weights = "best.pt"
    img_size = "640"
    confidence = "0.25"
    save_csv = "--save-csv"
    source = "0"
    cmd = [command, script, "--weights", weights, "--img", img_size, "--conf", confidence, save_csv, "--source", source]
    process = subprocess.Popen(cmd)

def analyze_results():
    global running
    global csv_file_path
    global received_message
    global client
    start_time = 0
    last_checked = 0
    fall_count = 0
    status = True

    while running:
        try:
            # Check received message from user
            if received_message == 'shutdown': 
                status = False  # shut down notification manually
                start_time = time.time()
                print('>> Temporary Turn Off Notification (20s)')
                received_message = None
            # Reset notification status after 10 mins (suppose)
            if status is False and (time.time() - start_time) >= 20:
                status = True
                fall_count = 0
                print('>> Turn On Notification')

            df = pd.read_csv(csv_file_path, header=None)
            if len(df) > last_checked:
                # Store all newly generated labels in list
                new_labels = df.iloc[last_checked:, 1]
                new_values = df.iloc[last_checked:, 2]
                last_checked = len(df)
                # Check the current label 
                for label, value in zip(new_labels, new_values):
                    if label == 'fall detected' and value >= 0.8:
                        fall_count += 1
                        print("Detected fall's frame: " + str(fall_count))
                        if fall_count == 3: 
                            send_notification(status)
                            fall_count = 0
                            break

        except FileNotFoundError:  # haven't started fall detection
            print("Waiting for file...")
        except pd.errors.EmptyDataError:  # empty loaded data
            #print("File is empty, waiting for data...")
            continue

def setup_threads():
    # Start three threads 
    model_thread = threading.Thread(target=run_detection_script)
    model_thread.start()
    analysis_thread = threading.Thread(target=analyze_results)
    analysis_thread.start()
    mqtt_thread = threading.Thread(target=mqtt_run)
    mqtt_thread.start()

def mqtt_run():
    global client, TOPIC_SUB
    mqtt.subscribe(client, TOPIC_SUB)
    client.on_subscribe = on_subscribe
    client.on_message = on_message
    client.loop_forever()
    

# Response
def send_notification(status):
    global client, TOPIC_PUB, information
    information = 'High-possible Fall Detected'
    if status is True:
        client.on_publish = on_publish
        mqtt.publish(client, TOPIC_PUB, information) 


# Callbacks function: 
def on_connect(client, userdata, flags, rc, v5config=None):  # check connection
    global BROKER_ADDRESS
    print(dt.now().strftime("%H:%M:%S.%f")[:-2] + " Connection to Broker '" + BROKER_ADDRESS + "': " + ack(rc))

def on_message(client, userdata, message,tmp=None):  # print received message
    global received_message
    received_message = message.payload.decode()
    print(dt.now().strftime("%H:%M:%S.%f")[:-2] + " Received Message `" + received_message + "` on Topic '"
        + message.topic + "' with QoS " + str(message.qos))
        
def on_publish(client, userdata, mid,tmp=None):  # indicate it is a publisher 
    global information, BROKER_ADDRESS
    print(dt.now().strftime("%H:%M:%S.%f")[:-2] + " Published `" + information + "` with message id "+ str(mid) + " to Broker '" + BROKER_ADDRESS + "'")
    
def on_subscribe(client, userdata, mid, qos,tmp=None):  # indicate it is a subscriber
    if isinstance(qos, list):
        qos_msg = str(qos[0])
    else:
        qos_msg = f"and Granted QoS {qos[0]}"
    print(dt.now().strftime("%H:%M:%S.%f")[:-2] + " Subscribed " + qos_msg) 

  
if __name__ == "__main__":
    running = True 
    received_message = None

    # Setup and connect mqtt
    VERSION = "3"  # 3.11 or 5.0
    MYTRANSPORT = 'tcp'
    TOPIC_PUB = 'pub/fall_detection' # as a subscriber and publisher
    TOPIC_SUB = 'sub/fall_detection'
    BROKER_ADDRESS = '192.168.137.229'
    PORT = 1883  # Websocket - 443, TCP - 8883(TLS)/1883(default)
    client = mqtt.setup_mqtt(VERSION, MYTRANSPORT)
    client.on_connect = on_connect
    mqtt.connect_broker(client, VERSION, BROKER_ADDRESS, PORT) 

    # Clear previous content in .csv file
    csv_file_path = os.path.join(os.path.dirname(__file__), 'yolov5', 'runs', 'detect', 'exp', 'predictions.csv')
    with open(csv_file_path, 'w', newline='') as csv_file:
        csv_writer = csv.writer(csv_file)
        # Writing an empty row to ensure the file is not entirely empty
        csv_writer.writerow([])

    # Set up multiple threads
    setup_threads()




