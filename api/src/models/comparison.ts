/**
 * Copyright 2018-2020 Pejman Ghorbanzade. All rights reserved.
 */

import {
  ComparisonModel,
  IComparisonDocument
} from '@weasel/schemas/comparison'
import logger from '@weasel/utils/logger'
import * as minio from '@weasel/utils/minio'

/**
 *
 */
export async function comparisonRemove(
  jobs: IComparisonDocument[]
): Promise<void> {
  try {
    // remove comparison results from object storage

    const removal = jobs.map((job) => minio.removeComparison(job.contentId))
    await Promise.all(removal)
    logger.debug('removed %d comparison results', jobs.length)

    // remove processed comparison jobs

    const jobIds = jobs.map((elem) => elem._id)
    await ComparisonModel.deleteMany({ _id: { $in: jobIds } })
    logger.debug('removed %d processed comparison jobs', jobs.length)
  } catch (err) {
    logger.warn('failed to remove comparison jobs: %s', err)
  }
}
