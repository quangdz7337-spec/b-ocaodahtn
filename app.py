from flask import Flask, request, jsonify, send_from_directory
from flask_cors import CORS
import json
import os

# =====================================================
# FLASK
# =====================================================
app = Flask(
    __name__,
    static_folder='frontend',
    static_url_path=''
)

CORS(app)

# =====================================================
# FILE
# =====================================================
DATA_FILE = 'data.json'
CONTROL_FILE = 'control.json'

# =====================================================
# DEFAULT SENSOR DATA
# =====================================================
default_data = {

    "temperature": 0,
    "humidity": 0,

    "currentL": 0,
    "currentF": 0,

    "presence": False,

    "light": False,
    "fan": False
}

# =====================================================
# DEFAULT CONTROL
# =====================================================
default_control = {

    "light": False,
    "fan": False
}

# =====================================================
# CREATE FILE IF NOT EXISTS
# =====================================================
if not os.path.exists(DATA_FILE):

    with open(DATA_FILE, 'w') as f:

        json.dump(default_data, f)

if not os.path.exists(CONTROL_FILE):

    with open(CONTROL_FILE, 'w') as f:

        json.dump(default_control, f)

# =====================================================
# FRONTEND
# =====================================================
@app.route('/')
def index():

    return send_from_directory(
        'frontend',
        'index.html'
    )

# =====================================================
# GET SENSOR DATA
# =====================================================
@app.route('/api/data', methods=['GET'])
def get_data():

    try:

        with open(DATA_FILE, 'r') as f:

            data = json.load(f)

        return jsonify(data)

    except Exception as e:

        print(e)

        return jsonify(default_data)

# =====================================================
# SAVE SENSOR DATA FROM ESP32
# =====================================================
@app.route('/api/data', methods=['POST'])
def save_data():

    data = request.json

    if not data:

        return jsonify({
            "error": "No data"
        }), 400

    with open(DATA_FILE, 'w') as f:

        json.dump(data, f)

    return jsonify({
        "message": "saved"
    }), 200

# =====================================================
# GET CONTROL FOR ESP32
# =====================================================
@app.route('/api/control', methods=['GET'])
def get_control():

    try:

        with open(CONTROL_FILE, 'r') as f:

            data = json.load(f)

        return jsonify(data)

    except Exception as e:

        print(e)

        return jsonify(default_control)

# =====================================================
# SAVE CONTROL FROM WEB
# =====================================================
@app.route('/api/control', methods=['POST'])
def save_control():

    data = request.json

    if not data:

        return jsonify({
            "error": "No control"
        }), 400

    with open(CONTROL_FILE, 'w') as f:

        json.dump(data, f)

    return jsonify({
        "message": "control updated"
    }), 200

# =====================================================
# MAIN
# =====================================================
if __name__ == '__main__':

    print("====================================")
    print("SERVER RUNNING PORT 5000")
    print("====================================")

    app.run(

        host='0.0.0.0',
        port=5000,
        debug=True
    )