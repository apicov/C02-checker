from flask import Flask,request,jsonify

app = Flask(__name__)

sensors_data = {"DATETIME": "","TEMP": 0.0, "RH":0.0, "CO2" : 0.0,
        "PEOPLE":0, "PM1_0":0, "PM2_5":0, "PM10":0, "TVOC":0,
         "ECO2":0, "RAW_H2":0, "RAW_ETHANOL":0, "BASELINE_ECO2":0,
         "BASELINE_TVOC":0 }

@app.route('/sensors/sensors', methods=['POST'])
def get_scd30_data():
    if request.method == 'POST':
       content = request.get_json(silent=True)
       sensors_data["DATETIME"] = content["DATETIME"]
       
       sensors_data["RH"] = content["RH"]
       sensors_data["TEMP"] = content["TEMP"]
       sensors_data["CO2"] = content["CO2"]
       
       sensors_data["PEOPLE"] = content["PEOPLE"]
       
       sensors_data["PM1_0"] = content["PM1_0"]
       sensors_data["PM2_5"] = content["PM2_5"]
       sensors_data["PM10"] = content["PM10"]
       
       sensors_data["TVOC"] = content["TVOC"]
       sensors_data["ECO2"] = content["ECO2"]
       sensors_data["RAW_H2"] = content["RAW_H2"]
       sensors_data["RAW_ETHANOL"] = content["RAW_ETHANOL"]
       sensors_data["BASELINE_ECO2"] = content["BASELINE_ECO2"]
       sensors_data["BASELINE_TVOC"] = content["BASELINE_TVOC"]
       

       print(sensors_data)
    return content

@app.route('/sensors/sensors', methods=['GET'])
def send_scd30_data():
    return sensors_data


app.run(host='0.0.0.0',port=8090)
