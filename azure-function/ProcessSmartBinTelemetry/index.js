
const https = require("https");
const crypto = require("crypto");

const TABLE_NAME = "SmartBinStatus";
const PARTITION_KEY = "SmartBins";
const ROW_KEY = "SmartBin001";

function getStorageCredentials() {
    const connection = process.env.ecosortbinstatus_STORAGE;

    if (!connection) {
        throw new Error("Missing ecosortbinstatus_STORAGE setting");
    }

    const parts = Object.fromEntries(
        connection.split(";")
            .filter(part => part.includes("="))
            .map(part => {
                const index = part.indexOf("=");
                return [
                    part.slice(0, index),
                    part.slice(index + 1)
                ];
            })
    );

    if (!parts.AccountName || !parts.AccountKey) {
        throw new Error("Invalid storage connection string");
    }

    return parts;
}

function saveToTable(data) {
    return new Promise((resolve, reject) => {
        try {
            const credentials = getStorageCredentials();
            const account = credentials.AccountName;
            const key = Buffer.from(
                credentials.AccountKey,
                "base64"
            );

            const entity = {
                PartitionKey: PARTITION_KEY,
                RowKey: ROW_KEY,
                BinStatus: String(data.binStatus || "UNKNOWN"),
                HandDetected: data.handDetected === true,
                LidStatus: String(data.lidStatus || "UNKNOWN")
            };

            if (
                data.fillPercentage !== null &&
                data.fillPercentage !== undefined &&
                Number.isFinite(Number(data.fillPercentage))
            ) {
                entity.FillPercentage = Number(data.fillPercentage);
            }

            const body = JSON.stringify(entity);
            const date = new Date().toUTCString();

            // Azure Table Storage entity URL
            const entityPath =
                `/${TABLE_NAME}` +
                `(PartitionKey='${PARTITION_KEY}',` +
                `RowKey='${ROW_KEY}')`;

            // SharedKeyLite signs the canonicalized table resource.
            // The resource path must match the request path.
            const canonicalizedResource =
                `/${account}${entityPath}`;

            const stringToSign =
                `${date}\n${canonicalizedResource}`;

            const signature = crypto
                .createHmac("sha256", key)
                .update(stringToSign, "utf8")
                .digest("base64");

            const options = {
                hostname: `${account}.table.core.windows.net`,
                path: entityPath,
                method: "PUT",
                headers: {
                    "x-ms-date": date,
                    "x-ms-version": "2019-02-02",
                    "Authorization":
                        `SharedKeyLite ${account}:${signature}`,
                    "Content-Type": "application/json",
                    "Accept": "application/json;odata=nometadata",
                    "Content-Length": Buffer.byteLength(body)
                }
            };

            const request = https.request(options, response => {
                let result = "";

                response.on("data", chunk => {
                    result += chunk;
                });

                response.on("end", () => {
                    if (
                        response.statusCode >= 200 &&
                        response.statusCode < 300
                    ) {
                        resolve();
                    } else {
                        reject(
                            new Error(
                                `Table Storage HTTP ${response.statusCode}: ${result}`
                            )
                        );
                    }
                });
            });

            request.setTimeout(15000, () => {
                request.destroy(
                    new Error("Table Storage request timed out")
                );
            });

            request.on("error", reject);

            request.write(body);
            request.end();

        } catch (error) {
            reject(error);
        }
    });
}

module.exports = async function (context, eventHubMessages) {
    const messages = Array.isArray(eventHubMessages)
        ? eventHubMessages
        : [eventHubMessages];

    for (const rawMessage of messages) {
        try {
            const data = typeof rawMessage === "string"
                ? JSON.parse(rawMessage)
                : rawMessage;

            if (!data || data.deviceId !== "SmartBin001") {
                context.log.warn("Ignoring unknown device");
                continue;
            }

            await saveToTable(data);

            context.log(
                `SmartBin001 saved successfully: ` +
                `${data.fillPercentage}% | ` +
                `${data.binStatus} | ` +
                `${data.lidStatus}`
            );

        } catch (error) {
            context.log.error(
                "Telemetry processing failed:",
                error
            );

            throw error;
        }
    }
};
