
import os
from datetime import timezone
from flask import Flask, jsonify, render_template
from azure.data.tables import TableServiceClient

app = Flask(__name__)

STORAGE_CONNECTION_STRING = os.environ.get(
    "AZURE_STORAGE_CONNECTION_STRING"
)

TABLE_NAME = "SmartBinStatus"
PARTITION_KEY = "SmartBins"
ROW_KEY = "SmartBin001"

table_client = None

def get_table_client():
    global table_client

    if table_client is None:
        if not STORAGE_CONNECTION_STRING:
            raise RuntimeError(
                "AZURE_STORAGE_CONNECTION_STRING is missing"
            )

        service = TableServiceClient.from_connection_string(
            STORAGE_CONNECTION_STRING
        )

        table_client = service.get_table_client(TABLE_NAME)

    return table_client


@app.route("/")
def home():
    return render_template("index.html")


@app.route("/api/status")
def get_status():
    try:
        table = get_table_client()

        entity = table.get_entity(
            partition_key=PARTITION_KEY,
            row_key=ROW_KEY
        )

        timestamp = entity.get("Timestamp")

        if timestamp:
            if timestamp.tzinfo is None:
                timestamp = timestamp.replace(
                    tzinfo=timezone.utc
                )
            last_updated = timestamp.isoformat()
        else:
            last_updated = None

        response = {
            "deviceId": ROW_KEY,
            "fillPercentage": entity.get(
                "FillPercentage"
            ),
            "binStatus": entity.get(
                "BinStatus", "UNKNOWN"
            ),
            "handDetected": entity.get(
                "HandDetected", False
            ),
            "lidStatus": entity.get(
                "LidStatus", "UNKNOWN"
            ),
            "lastUpdated": last_updated
        }

        result = jsonify(response)
        result.headers["Cache-Control"] = "no-store"
        return result

    except Exception:
        app.logger.exception("Failed to retrieve Smart Bin data")
        return jsonify({
            "error": "Unable to retrieve Smart Bin data"
        }), 503


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8000)
