"""Reject incomplete QtTest XML even when a child process reports exit zero."""
import json
import xml.etree.ElementTree as ET

def verify_xml(path, output, backend):
    record = {'xml': str(path), 'backend': backend, 'complete': False}
    try:
        root = ET.parse(path).getroot()
        if root.tag != 'TestCase':
            raise ValueError('Expected QtTest TestCase root')
        rows = []
        for function in root.findall('TestFunction'):
            for incident in function.findall('Incident'):
                rows.append({'function': function.attrib['name'],
                             'data_tag': incident.findtext('DataTag', ''),
                             'result': incident.attrib['type']})
        if not any(row['function'] == 'cleanupTestCase' and row['result'] == 'pass' for row in rows):
            raise ValueError('Missing successful cleanupTestCase sentinel')
        if not rows or any(row['result'] != 'pass' for row in rows):
            raise ValueError('QtTest contains failed, skipped or unexpected incidents')
        if root.findall('.//Message[@type="skip"]'):
            raise ValueError('QtTest contains skipped rows')
        record.update(complete=True, tests=rows, count=len(rows), failed=0, skipped=0)
    except (ET.ParseError, ValueError, OSError) as error:
        record['error'] = str(error)
        output.write_text(json.dumps(record, indent=2) + '\n')
        raise SystemExit('QtTest log validation failed: ' + str(path) + ': ' + str(error))
    output.write_text(json.dumps(record, indent=2) + '\n')
    return record
