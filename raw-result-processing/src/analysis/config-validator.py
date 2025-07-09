from jsonschema import validate
import json
import sys

def validateConfig(schemaFile, configFile):
    with open(schemaFile) as schemaFile:
        schema = json.load(schemaFile)

    with open(configFile) as configFile:
        config = json.load(configFile)

    validate(schema=schema, instance=config)

if (len(sys.argv) != 2):
    print("Usage: python config-validator.py <config-file>")
    sys.exit(1)

validateConfig("./analysis-config-schema.json", sys.argv[1])
