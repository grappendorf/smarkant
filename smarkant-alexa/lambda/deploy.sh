#!/bin/bash
#
# Package and upload the Lambda function to AWS

package=/tmp/smarkant-lambda.zip
trap "rm -f $package" EXIT
zip -r $package *
aws lambda update-function-code --function-name Smarkant --zip-file fileb://$package
