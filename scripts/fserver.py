from flask import Flask,request,jsonify
import sqlalchemy
from sqlalchemy import create_engine
import json
import pandas as pd
import numpy as np
#code below is neeeded to make sqalchemy accept numpy ints in dataframe
import psycopg2
from psycopg2.extensions import register_adapter, AsIs
psycopg2.extensions.register_adapter(np.int64, psycopg2._psycopg.AsIs)



app = Flask(__name__)

sensors_sample = {}

#load login data
with open("private_data.json", "r") as read_file:
        p_data = json.load(read_file)

DATABASE = p_data['database'] 
USER = p_data['user']
PASSWORD = p_data['pwd']
HOST = p_data['host']
PORT = p_data['port']
TABLE = p_data['table']

#connect to database
try:
    db = create_engine(f"postgresql://{USER}:{PASSWORD}@{HOST}:{PORT}/{DATABASE}")
    conn = db.connect()
except:
    print('Could not open database')



@app.route('/sensors/sensors', methods=['POST'])
def get_scd30_data():
    global sensors_sample, db, conn
    if request.method == 'POST':
       content = request.get_json(silent=True)
       sensors_sample = content.copy()

       #add incomming data to database
       try:
           air_df = pd.DataFrame(sensors_sample, index=[0])
           air_df['DATETIME'] = pd.to_datetime(air_df['DATETIME'], format='%Y-%m-%d %H:%M:%S')
           air_df.to_sql(TABLE, conn,schema='public',index=False, if_exists= 'append')
           conn.commit()
       except:
           print("problem writing to database")

       #print database last content, temporary?
       print(pd.read_sql_query(sqlalchemy.text('select * from ' + TABLE), con=conn).tail())

       print(sensors_sample)
    return sensors_sample

@app.route('/sensors/sensors', methods=['GET'])
def send_scd30_data():
    return sensors_sample


app.run(host='0.0.0.0',port=8090)
