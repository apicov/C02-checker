from flask import Flask,request,jsonify

app = Flask(__name__)

sensors_data = {}

@app.route('/sensors/sensors', methods=['POST'])
def get_scd30_data():
    if request.method == 'POST':
       content = request.get_json(silent=True)
       sensors_data = content.copy()

       print(sensors_data)
    return content

@app.route('/sensors/sensors', methods=['GET'])
def send_scd30_data():
    return sensors_data


app.run(host='0.0.0.0',port=8090)
