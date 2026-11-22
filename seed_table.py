
import os
from azure.data.tables import TableClient

connection_string = os.getenv("AZURE_STORAGE_CONNECTION_STRING")

if not connection_string:
    raise RuntimeError("Set AZURE_STORAGE_CONNECTION_STRING first")

table = TableClient.from_connection_string(
    conn_str=connection_string,
    table_name="SmartBinStatus"
)

entity = {
    "PartitionKey": "SmartBins",
    "RowKey": "SmartBin001",
    "FillPercentage": 10,
    "BinStatus": "Empty",
    "HandDetected": True,
    "LidStatus": "Open",
    "LastUpdated": "2026-11-13T011:56:00Z"
}

table.upsert_entity(entity=entity, mode="replace")

print("Smart Bin test status saved successfully!")
