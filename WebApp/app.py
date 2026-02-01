from flask import Flask, render_template, request, redirect, url_for
from flask_sqlalchemy import SQLAlchemy
import subprocess
from flask import flash


app = Flask(__name__)
app.secret_key = "TODO"
app.config["SQLALCHEMY_DATABASE_URI"] = "sqlite:///items.db"
app.config["SQLALCHEMY_TRACK_MODIFICATIONS"] = False

db = SQLAlchemy(app)

# Model
class Item(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    name = db.Column(db.String(100), nullable=False)
    state = db.Column(db.Boolean, default=False)  # ON/OFF
    ip = db.Column(db.String(15), nullable=True)  # Optional IPv4

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

        new_item = Item(name=name, ip=ip)
        db.session.add(new_item)
        db.session.commit()

        flash("Device added successfully!", "success")
        return redirect(url_for("device_detail", id=new_item.id))

    return render_template("add_device.html")

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
        db.session.commit()
        flash("Device updated successfully!", "success")
        return redirect(url_for("device_detail", id=item.id))

    return render_template("edit.html", item=item)

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
    return render_template("device.html", item=item)


if __name__ == "__main__":
    app.run(debug=True)