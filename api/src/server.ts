/**
 * Copyright 2018-2020 Pejman Ghorbanzade. All rights reserved.
 */

import fs from 'fs'
import compression from 'compression'
import cookieParser from 'cookie-parser'
import cors from 'cors'
import express from 'express'
import hidePoweredBy from 'hide-powered-by'
import nocache from 'nocache'
import moduleAlias from 'module-alias'

moduleAlias.addAliases({
  '@weasel/controllers': `${__dirname}/controllers`,
  '@weasel/models': `${__dirname}/models`,
  '@weasel/routes': `${__dirname}/routes`,
  '@weasel/schemas': `${__dirname}/schemas`,
  '@weasel/utils': `${__dirname}/utils`
})

import {
  analyticsService,
  autosealService,
  reportingService,
  retentionService
} from './services'
import { setupSuperuser } from './startup'
import router from './routes'
import { MetaModel } from '@weasel/schemas/meta'
import { config, configMgr } from '@weasel/utils/config'
import logger from '@weasel/utils/logger'
import { makeConnectionMinio } from '@weasel/utils/minio'
import { makeConnectionMongo, shutdownMongo } from '@weasel/utils/mongo'
import { makeConnectionRedis, shutdownRedis } from '@weasel/utils/redis'
import { connectToServer } from '@weasel/utils/routing'

/**
 *
 */

const app = express()

app.use(cors({ origin: true, credentials: true, preflightContinue: true }))
app.use(cookieParser(config.auth.cookieSecret))
app.use(nocache())
app.use(hidePoweredBy())
app.use(compression())

// in cloud-hosted deployments where backend runs behind a reverse proxy,
// configure nginx to trust the proxy and infer ip address from upstream.
if (config.isCloudHosted) {
  app.set('trust proxy', ['loopback', 'uniquelocal'])
}

app.use('/', router)

app.use((req, res, next) => {
  logger.warn('invalid route %s', req.originalUrl)
  return res.status(404).json({ errors: ['invalid route'] })
})

app.use((err, req, res, next) => {
  const level = (err.status || 500) === 500 ? 'error' : 'warn'
  logger.log(level, '%j', err)
  res.status(err.status || 500).json({ errors: err.errors })
})

let server
async function launch(application) {
  // make sure we can connect to object storage
  if (!(await connectToServer(makeConnectionMinio, 'object storage'))) {
    process.exit(1)
  }

  // make sure we can connect to database
  if (!(await connectToServer(makeConnectionMongo, 'database'))) {
    process.exit(1)
  }

  // make sure we can connect to cache server
  if (!(await connectToServer(makeConnectionRedis, 'cache server'))) {
    process.exit(1)
  }

  if ((await MetaModel.countDocuments()) === 0) {
    await MetaModel.create({})
    logger.info('created meta document with default values')
  }

  if (config.isCloudHosted) {
    logger.info('running in cloud-hosted mode')
  }

  if (!configMgr.hasMailTransport()) {
    logger.warn('mail server not configured')
  }

  if (!fs.existsSync(config.samples.directory)) {
    logger.warn('samples directory not found at %s', config.samples.directory)
  }

  if (!config.samples.enabled) {
    logger.warn('feature to submit sample data is disabled')
  }

  // setup analytics service that performs background data processing
  // and populates batches and elements with information to be provided
  // to the user.
  setInterval(analyticsService, config.services.analytics.checkInterval * 1000)

  // setup auto-sealing service that periodically identifies recently
  // submitted batches and seals them to prevent future submission of
  // results to them.
  setInterval(autosealService, config.services.autoseal.checkInterval * 1000)

  // setup data retention policy enforcement service that periodically
  // identifies and prunes messages to free up disk space in databases
  // and local filesystem.
  setInterval(retentionService, config.services.retention.checkInterval * 1000)

  // setup batch comparison reporting service that periodically
  // identifies new batch comparison results and reports them
  // to subscribed users.
  setInterval(reportingService, config.services.reporting.checkInterval * 1000)

  // create a superuser if this platform was just setup
  await setupSuperuser()

  server = application.listen(config.express.port, () => {
    logger.info('listening on port %d', server.address().port)
  })
}

async function shutdown(): Promise<void> {
  await shutdownMongo()
  await shutdownRedis()
}

process.once('SIGUSR2', () => {
  const kill = () => process.kill(process.pid, 'SIGUSR2')
  shutdown()
    .then(kill)
    .catch((err) => {
      logger.warn('backend failed to shutdown gracefully: %O', err)
      kill()
    })
})

process.on('SIGINT', () => {
  const kill = () => process.exit(0)
  shutdown()
    .then(kill)
    .catch((err) => {
      logger.warn('backend failed to shutdown gracefully: %O', err)
      kill()
    })
})

launch(app)
