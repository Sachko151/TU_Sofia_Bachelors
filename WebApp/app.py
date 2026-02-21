from flask import Flask, render_template, request, redirect, url_for, jsonify
from flask_sqlalchemy import SQLAlchemy
import subprocess
from flask import flash
from flask import Flask, send_file
import enum
from sqlalchemy import Enum
import threading


app = Flask(__name__)
app.secret_key = "TODO"
app.config["SQLALCHEMY_DATABASE_URI"] = "sqlite:///items.db"
app.config["SQLALCHEMY_TRACK_MODIFICATIONS"] = False

db = SQLAlchemy(app)

devices = []
scan_running = False
lock = threading.Lock()

class ECUType(enum.Enum):
    GENERIC = "generic"
    AC = "ac"
    LIGHTS = "lights"
    HEATING = "engine"
    DOOR = "doors"
    TEMPERATURE_SENSOR = "sensor"

# Model
class Item(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    name = db.Column(db.String(100), nullable=False)
    state = db.Column(db.Boolean, default=False)
    ip = db.Column(db.String(15), nullable=True)

    ecu_type = db.Column(
        Enum(ECUType),
        nullable=False,
        default=ECUType.GENERIC
    )

# Create DB
with app.app_context():
    db.create_all()

# READ + CREATE
@app.route("/")
def index():
    items = Item.query.all()
    return render_template("index.html", items=items)

#CREATE
@app.route("/add", methods=["GET", "POST"])
def add_device():
    if request.method == "POST":
        name = request.form.get("name")
        ip = request.form.get("ip")
        ecu_type = ECUType(request.form.get("ecu_type"))

        new_item = Item(name=name, ip=ip, ecu_type=ecu_type)
        db.session.add(new_item)
        db.session.commit()

        flash("Device added successfully!", "success")
        return redirect(url_for("device_detail", id=new_item.id))

    return render_template("add_device.html", ecu_types=ECUType)


@app.route("/ping/<int:id>")
def ping(id):
    item = Item.query.get_or_404(id)
    ip = item.ip
    if not ip:
        output = "No IP configured for this device."
    else:
        try:
            result = subprocess.run(
                ["ping", "-c", "4", ip],  # Unix, for Windows use ["ping", "-n", "4", ip]
                capture_output=True,
                text=True,
                timeout=5
            )
            output = result.stdout
        except Exception as e:
            output = str(e)

    return render_template("ping.html", output=output, item=item)

# UPDATE
@app.route("/edit/<int:id>", methods=["GET", "POST"])
def edit(id):
    item = Item.query.get_or_404(id)

    if request.method == "POST":
        item.name = request.form.get("name")
        item.ip = request.form.get("ip")
        item.ecu_type = ECUType(request.form.get("ecu_type"))
        db.session.commit()
        flash("Device updated successfully!", "success")
        return redirect(url_for("device_detail", id=item.id))

    return render_template("edit.html", item=item, ecu_types=ECUType)

@app.route("/toggle/<int:id>", methods=["POST"])
def toggle_device(id):
    item = Item.query.get_or_404(id)
    item.state = not item.state
    db.session.commit()
    # Here you can call your IoT API or GPIO control
    flash(f"{item.name} turned {'ON' if item.state else 'OFF'}")
    return redirect(url_for("index"))

# DELETE
@app.route("/delete/<int:id>")
def delete(id):
    item = Item.query.get_or_404(id)
    db.session.delete(item)
    db.session.commit()
    return redirect(url_for("index"))

# DEVICE DETAILS
@app.route("/device/<int:id>")
def device_detail(id):
    item = Item.query.get_or_404(id)
    ip = item.ip
    return render_template("device.html", item=item)

@app.route("/scan/")
def scan():
    global scan_running
    return render_template("scan.html", scan_running=scan_running)

@app.route("/scan/status")
def scan_status():
    with lock:
        return jsonify({
            "running": scan_running,
            "devices": devices
        })

@app.route("/scan/action")
def scan_action():
    global scan_running

    if not scan_running:
        threading.Thread(target=scan_worker, daemon=True).start()

    return render_template("scan_action.html")

@app.route("/ota")
def download_bin():
    return send_file(
        "ota_update/firmware.bin",   # path to your .bin file
        as_attachment=True,     # forces download
        download_name="firmware.bin",  # filename user sees
        mimetype="application/octet-stream"
    )
@app.route("/version")
def version():
    return "1.0.1"   # change this when you upload new firmware


def scan_worker():
    global devices, scan_running
    print("Scanning begin!")

    # Set initial state
    with lock:
        scan_running = True
        devices = []

    for x in range(100, 150):
        ip = f"192.168.0.{x}"
        print("Scanning:", ip)

        try:
            subprocess.run(
                ["../OrangePI_SoC_Code/udp_tx", "-s", ip]
            )

            result = subprocess.run(
                ["../OrangePI_SoC_Code/udp_rx", "-s"],
                capture_output=True,
                text=True,
                timeout=2
            )

            for line in result.stdout.splitlines():
                line = line.strip()
                if line:
                    with lock:
                        if line not in devices:
                            devices.append(line)

        except subprocess.TimeoutExpired:
            print("No response from", ip)
            continue

    # Mark finished (ONLY ONCE)
    with lock:
        scan_running = False

    print("Scanning complete!")

if __name__ == "__main__":
    app.run(debug=True)