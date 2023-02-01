from flask import Flask,request,jsonify

app = Flask(__name__)

scd30_data = {"temperature": 0.0, "rHumidity":0.0, "CO2" : 0.0}

@app.route('/sensors/scd30', methods=['POST'])
def get_scd30_data():
    if request.method == 'POST':
       content = request.get_json(silent=True)
       scd30_data["temperature"] = content["TEMP"]
       scd30_data["rHumidity"] = content["RH"]
       scd30_data["CO2"] = content["CO2"]

       print(scd30_data)
    return content

@app.route('/sensors/scd30', methods=['GET'])
def send_scd30_data():
    return scd30_data


app.run(host='0.0.0.0',port=8090)
