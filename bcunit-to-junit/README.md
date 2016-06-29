# bcunit-to-junit


Convert BCUnit xml test report to JUnit schema and make it easy to integrate with Jenkins CI xUnit plugin
--

## Purpose

[BCUnit](https://github.com/BelledonneCommunications/bcunit/) is a uniting framework written in pure C. Like other xUnit framework, BCUnit can generate xml test reports. This XSL is able to convert BCUnit xml report to popular JUnit schema and makes it easy to integrate BCUnit with [Jenkin CI](http://jenkins-ci.org/).


## Use with Jenkins CI

1. Install [xUnit plugin](https://wiki.jenkins-ci.org/display/JENKINS/xUnit+Plugin) to your Jenkins CI
2. Configure project's post build action to "Publish xUnit test result report"
3. Choose "custom tool" as report type
4. Specify BCUnit report file pattern
5. Specify the path to <code>bcunit-to-junit.xsl</code>

And there you GO!

