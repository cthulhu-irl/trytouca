/**
 * Copyright 2018-2020 Pejman Ghorbanzade. All rights reserved.
 */

import { NextFunction, Request, Response } from 'express'

import { addSampleData } from '@/models/sampleData'
import { IUser } from '@/schemas/user'
import logger from '@/utils/logger'

/**
 *
 */
export async function platformAccountPopulate(
  req: Request,
  res: Response,
  next: NextFunction
) {
  const account = res.locals.account as IUser
  const user = res.locals.user as IUser
  logger.debug(
    '%s: populating account %s with sample data',
    user.username,
    account.username
  )

  await addSampleData(account)

  logger.info(
    '%s: populate account %s with sample data',
    user.username,
    account.username
  )

  return res.status(204).send()
}
