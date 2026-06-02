#=================================================================#
#                        INCLUDES                                 #
#=================================================================#
from flask import Flask, render_template, request, redirect, url_for, jsonify
from flask_sqlalchemy import SQLAlchemy
import subprocess
from flask import flash
from flask import Flask, send_file
import enum
from sqlalchemy import Enum
import threading
import re
import glob
import cv2
import time
import os

camera_threads = {}
camera_stop_flags = {}
FRAME_FOLDER = "static/frames"

#=================================================================#
#                        GLOBALS                                  #
#=================================================================#
app = Flask(__name__)
app.secret_key = "TODO"
app.config["SQLALCHEMY_DATABASE_URI"] = "sqlite:///items.db"
app.config["SQLALCHEMY_TRACK_MODIFICATIONS"] = False
db = SQLAlchemy(app)
devices = []
devices_last = []
scan_running = False
lock = threading.Lock()
camera_processes = {}

#=================================================================#
#                        CLASSES                                  #
#=================================================================#
class ECUType(enum.Enum):
    GENERIC = "generic"
    AC = "Artifical Cooling"
    LIGHTS = "lights"
    HEATING = "Heating System"
    DOOR = "Door Control"
    TEMPERATURE_SENSOR = "sensor"
    IP_CAMERA = "ip_camera"

# Model
class Item(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    name = db.Column(db.String(100), nullable=False)
    state = db.Column(db.Boolean, default=False)
    ip = db.Column(db.String(15), nullable=True)
    rtsp_url = db.Column(db.String(255), nullable=True)

    ecu_type = db.Column(
        Enum(ECUType),
        nullable=False,
        default=ECUType.GENERIC
    )

# Create DB
with app.app_context():
    db.create_all()

#=================================================================#
#                         INDEX ROUTE                             #
#=================================================================#
@app.route("/")
def index():
    items = Item.query.all()
    return render_template("index.html", items=items)

#=================================================================#
#                         CREATE ROUTE                            #
#=================================================================#
@app.route("/add", methods=["GET", "POST"])
def add_device():
    if request.method == "POST":
        name = request.form.get("name")
        ip = request.form.get("ip")
        ecu_type = ECUType(request.form.get("ecu_type"))
        rtsp_url = request.form.get("rtsp_url")

        new_item = Item(name=name, ip=ip, ecu_type=ecu_type, rtsp_url=rtsp_url)
        db.session.add(new_item)
        db.session.commit()

        flash("Device added successfully!", "success")
        return redirect(url_for("device_detail", id=new_item.id))

    return render_template("add_device.html", ecu_types=ECUType)

#=================================================================#
#                  PING SPECIFIC DEVICE ROUTE                     #
#=================================================================#
@app.route("/ping/<int:id>")
def ping(id):
    item = Item.query.get_or_404(id)
    ip = item.ip
    if not ip:
        output = "No IP configured for this device."
    else:
        try:
            subprocess.run(
                ["../OrangePI_SoC_Code/udp_tx", "-e", ip],
                timeout=1
            )

            # Wait for response
            result = subprocess.run(
                ["../OrangePI_SoC_Code/udp_rx", "-e"],
                capture_output=True,
                text=True,
                timeout=5
            )
            output = result.stdout.strip()
        except subprocess.TimeoutExpired:
            output = f"No response from, {ip}"
            print("No response from", ip)

    return render_template("ping.html", output=output, item=item)

#=================================================================#
#                       UPDATE ROUTE                              #
#=================================================================#
@app.route("/edit/<int:id>", methods=["GET", "POST"])
def edit(id):
    item = Item.query.get_or_404(id)

    if request.method == "POST":
        item.name = request.form.get("name")
        item.ip = request.form.get("ip")
        item.ecu_type = ECUType(request.form.get("ecu_type"))
        item.rtsp_url = request.form.get("rtsp_url")
        db.session.commit()
        flash("Device updated successfully!", "success")
        return redirect(url_for("device_detail", id=item.id))

    return render_template("edit.html", item=item, ecu_types=ECUType)

#=================================================================#
#                       TOGGLE OFF                                #
#=================================================================#
@app.route("/toggle/<int:id>", methods=["POST"])
def toggle_device(id):
    item = Item.query.get_or_404(id)
    item.state = not item.state
    db.session.commit()
    # Here you can call your IoT API or GPIO control
    flash(f"{item.name} turned {'ON' if item.state else 'OFF'}")
    return redirect(url_for("index"))

#=================================================================#
#                       DELETE ROUTE                              #
#=================================================================#
@app.route("/delete/<int:id>")
def delete(id):
    item = Item.query.get_or_404(id)
    db.session.delete(item)
    db.session.commit()
    return redirect(url_for("index"))

#=================================================================#
#                     DEVICE ACTION ROUTE                         #
#=================================================================#
@app.route("/device/<int:id>/action", methods=["POST"])
def device_action(id):

    item = Item.query.get_or_404(id)

    action = request.form.get("action")
    second_action = None

    if not action:
        return jsonify({"error": "Missing action"}), 400

    if not item.ip:
        return jsonify({"error": "Device has no IP"}), 400
    
    if action == "set_brightness":
        second_action = request.form.get("brightness")
        print(f"{action}:{second_action}")
    elif action == "set_temp":
        second_action = request.form.get("temperature")
        print(f"{action}:{second_action}")
    elif action == "set_fan":
        second_action = request.form.get("fan_speed")
        print(f"{action}:{second_action}")
    elif action == "set_mode":
        second_action = request.form.get("ac_mode")
        print(f"{action}:{second_action}")
    elif action == "set_swing":
        second_action = request.form.get("swing_mode")
        print(f"{action}:{second_action}")
    elif action == "set_lights_mode":
        second_action = request.form.get("light_mode")
        print(f"{action}:{second_action}")
    elif action == "set_heating_temp":
        second_action = request.form.get("temperature")
        print(f"{action}:{second_action}")
    elif action == "set_heating_mode":
        second_action = request.form.get("heating_mode")
        print(f"{action}:{second_action}")
    else:
        pass

    COMMANDS = {
        ####UNIVERSAL######
        "healthcheck": "-v",
        ###FAN#####
        "ac_on": "-q",
        "ac_off": "-w",
        "set_fan": "-sfan",
        "set_temp": "-stemp",
        "set_fan": "-sfan",
        "set_mode": "-smode",
        "set_swing": "-swing",
        "turbo_mode": "-t",
        "eco_mode": "-y",
        "sleep_mode": "-o",
        ###LIGHTS#####
        "lights_on": "-u",
        "lights_off": "-i",
        "set_brightness": "-lbright",
        "set_lights_mode": "-slmode",
        ###HEATING#####
        "heating_on": "-h",
        "heating_off": "-a",
        "set_heating_temp": "-shtemp",
        "set_heating_mode": "-shmode",
        ###Smart Door Control#####
        "lock": "-d",
        "unlock": "-f",
        "ring_doorbell": "-g",

    }

    if action not in COMMANDS:
        return jsonify({"error": f"Invalid action {action}"}), 400

    print(f"Executing command: {action}")

    if action == "healthcheck":
        try:
            # Send healthcheck request
            subprocess.run(
                [
                    "../OrangePI_SoC_Code/udp_tx",
                    COMMANDS[action],
                    item.ip
                ],
                timeout=2,
                check=True
            )

            # Wait for reply
            result = subprocess.run(
                ["../OrangePI_SoC_Code/udp_rx", "-s"],
                capture_output=True,
                text=True,
                timeout=2
            )

            response = result.stdout.strip()

            return jsonify({
                "success": True,
                "response": response
            })

        except subprocess.TimeoutExpired:
            return jsonify({
                "success": False,
                "error": "Healthcheck timeout"
            }), 504
    else:

        if second_action != None:
            cmd = [
                "../OrangePI_SoC_Code/udp_tx",
                COMMANDS[action],
                second_action,
                item.ip
            ]
        else:
            cmd = [
                "../OrangePI_SoC_Code/udp_tx",
                COMMANDS[action],
                item.ip
            ]

        try:
            subprocess.run(cmd, timeout=2)

            return redirect(url_for("device_detail", id=id))

        except subprocess.TimeoutExpired:
            return jsonify({
                "success": False,
                "error": "Command timeout"
            }), 504


#=================================================================#
#                       RESET ROUTE?                              #
#=================================================================#
@app.route("/request_reset/<ip>")
def request_reset(ip):
    subprocess.run(["../OrangePI_SoC_Code/udp_tx", "-r", ip])
    return redirect(url_for("index"))

#=================================================================#
#                       DEVICE DETAILS                            #
#=================================================================#
@app.route("/device/<int:id>")
def device_detail(id):
    item = Item.query.get_or_404(id)

    if item.ecu_type == ECUType.IP_CAMERA and item.rtsp_url:
        start_camera_stream(item.id, item.rtsp_url)

    return render_template("device.html", item=item)

#=================================================================#
#                       SCAN ROUTE PAGE                           #
#=================================================================#
@app.route("/scan/")
def scan():
    global scan_running
    return render_template("scan.html", scan_running=scan_running, devices_last=devices_last)

#=================================================================#
#               CURRENT SCAN STATUS WORKER ROUTE                  #
#=================================================================#
@app.route("/scan/status")
def scan_status():
    with lock:
        return jsonify({
            "running": scan_running,
            "devices": devices
        })
#=================================================================#
#                 LAST SCANNED DEVICES ROUTE                      #
#=================================================================#    
@app.route("/scan/last_scan")
def last_scan():
    return render_template("last_scan.html", devices_last=devices_last)

#=================================================================#
#                    SCAN BEGIN ROUTE                             #
#=================================================================#  
@app.route("/scan/action")
def scan_action():
    global scan_running

    if not scan_running:
        threading.Thread(target=scan_worker, daemon=True).start()

    return render_template("scan_action.html")

#=================================================================#
#                ESP FIRMWARE FOR OTA ROUTE                       #
#=================================================================#
@app.route("/ota")
def download_bin():
    return send_file(
        "ota_update/firmware.bin",   # path to your .bin file
        as_attachment=True,     # forces download
        download_name="firmware.bin",  # filename user sees
        mimetype="application/octet-stream"
    )

#=================================================================#
#            ESP FIRMWARE VERSION STRING FOR OTA ROUTE            #
#=================================================================#
@app.route("/version")
def version():
    return "1.0.1"   # change this when you upload new firmware

#=================================================================#
#               FUNCTION WORKER FOR BACKGROUND SCAN               #
#=================================================================#
def scan_worker():
    global devices, scan_running
    print("Scanning begin!")

    # Set initial state
    with lock:
        scan_running = True
        devices = []

    local_ip = get_local_ip()

    if not local_ip:
        print("Could not determine local IP")
        return

    parts = local_ip.split(".")
    base_ip = f"{parts[0]}.{parts[1]}.{parts[2]}"

    for y in range(1, 255):
        ip = f"{base_ip}.{y}"
        print("Scanning:", ip)

        try:
            subprocess.run(
                ["../OrangePI_SoC_Code/udp_tx", "-s", ip]
            )

            result = subprocess.run(
                ["../OrangePI_SoC_Code/udp_rx", "-s"],
                capture_output=True,
                text=True,
                timeout=0.5
            )

            for line in result.stdout.splitlines():
                line = line.strip()
                if line:
                    with lock:
                        if line not in devices:
                            devices.append(line)
                            devices_last.append(line)

        except subprocess.TimeoutExpired:
            print("No response from", ip)
            continue

    # Mark finished (ONLY ONCE)
    with lock:
        scan_running = False

    print("Scanning complete!")

def get_local_ip():
    result = subprocess.run(["ip", "a"], capture_output=True, text=True)
    output = result.stdout

    # Match something like 192.168.x.y
    match = re.search(r"inet (192\.168\.\d+\.\d+)", output)
    if match:
        return match.group(1)
    return None

def camera_worker(device_id, rtsp_url, stop_flag):
    cap = cv2.VideoCapture(rtsp_url)

    if not cap.isOpened():
        print(f"[{device_id}] Failed to connect to camera")
        return

    save_dir = os.path.join(FRAME_FOLDER, str(device_id))
    os.makedirs(save_dir, exist_ok=True)

    latest_path = os.path.join(save_dir, "frame_latest.jpg")

    print(f"[{device_id}] Camera started")

    while not stop_flag.is_set():

        ret, frame = cap.read()

        if not ret:
            print(f"[{device_id}] Frame read failed")
            break

        # ALWAYS overwrite same file
        cv2.imwrite(latest_path, frame)

        time.sleep(0.5)  # 1 FPS snapshot

    cap.release()
    print(f"[{device_id}] Camera stopped")

def stop_camera_stream(device_id):
    if device_id in camera_stop_flags:
        camera_stop_flags[device_id].set()

    if device_id in camera_threads:
        camera_threads.pop(device_id, None)

    camera_stop_flags.pop(device_id, None)

    print(f"Stopped camera {device_id}")

def start_camera_stream(device_id, rtsp_url):

    if device_id in camera_threads:
        print(f"Camera {device_id} already running")
        return

    stop_flag = threading.Event()
    camera_stop_flags[device_id] = stop_flag

    thread = threading.Thread(
        target=camera_worker,
        args=(device_id, rtsp_url, stop_flag),
        daemon=True
    )

    camera_threads[device_id] = thread
    thread.start()

    print(f"Started camera thread for device {device_id}")
#=================================================================#
#                          MAIN                                   #
#=================================================================#
if __name__ == "__main__":
    os.makedirs(HLS_FOLDER, exist_ok=True)
    app.run(debug=True)