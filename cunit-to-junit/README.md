# cunit-to-junit


Convert CUnit xml test report to JUnit schema and make it easy to integrate with Jenkins CI xUnit plugin
--

## Purpose

[CUnit](http://cunit.sourceforge.net/) is a uniting framework written in pure C. Like other xUnit framework, CUnit can generate xml test reports. This XSL is able to convert CUnit xml report to popular JUnit schema and makes it easy to integrate CUnit with [Jenkin CI](http://jenkins-ci.org/).


## Use with Jenkins CI

1. Install [xUnit plugin](https://wiki.jenkins-ci.org/display/JENKINS/xUnit+Plugin) to your Jenkins CI
2. Configure project's post build action to "Publish xUnit test result report"
3. Choose "custom tool" as report type
4. Specify CUnit report file pattern
5. Specify the path to <code>cunit-to-junit.xsl</code>

And there you GO!

