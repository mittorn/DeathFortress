#include "precompiled.h"

/*
* Globals initialization
*/
sv_blending_interface_t svBlending =
{
	SV_BLENDING_INTERFACE_VERSION,
	SV_StudioSetupBones
};
//long long aaa [65536];
server_studio_api_t IEngineStudio;
//long long bbb[65536];
studiohdr_t *g_pstudiohdr;

float (*g_pRotationMatrix)[3][4];
float (*g_pBoneTransform)[128][3][4];

int ExtractBbox(void *pmodel, int sequence, float *mins, float *maxs)
{
	studiohdr_t *pstudiohdr = (studiohdr_t *)pmodel;

	if (!pstudiohdr)
	{
		return 0;
	}

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);

	mins[0] = pseqdesc[sequence].bbmin[0];
	mins[1] = pseqdesc[sequence].bbmin[1];
	mins[2] = pseqdesc[sequence].bbmin[2];

	maxs[0] = pseqdesc[sequence].bbmax[0];
	maxs[1] = pseqdesc[sequence].bbmax[1];
	maxs[2] = pseqdesc[sequence].bbmax[2];

	return 1;
}

int LookupActivity(void *pmodel, entvars_t *pev, int activity)
{
	studiohdr_t *pstudiohdr = (studiohdr_t *)pmodel;

	if (!pstudiohdr)
	{
		return 0;
	}

	mstudioseqdesc_t *pseqdesc;

	int i;
	int weightTotal = 0;
	int activitySequenceCount = 0;
	int weight = 0;
	int select;

	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);

	for (i = 0; i < pstudiohdr->numseq; ++i)
	{
		if (pseqdesc[i].activity == activity)
		{
			weightTotal += pseqdesc[i].actweight;
			++activitySequenceCount;
		}
	}

	if (activitySequenceCount > 0)
	{
		if (weightTotal)
		{
			int which = RANDOM_LONG(0, weightTotal - 1);

			for (i = 0; i < pstudiohdr->numseq; ++i)
			{
				if (pseqdesc[i].activity == activity)
				{
					weight += pseqdesc[i].actweight;

					if (weight > which)
					{
						return i;
					}
				}
			}
		}
		else
		{
			select = RANDOM_LONG(0, activitySequenceCount - 1);

			for (i = 0; i < pstudiohdr->numseq; ++i)
			{
				if (pseqdesc[i].activity == activity)
				{
					if (select == 0)
					{
						return i;
					}

					--select;
				}
			}
		}
	}

	return ACT_INVALID;
}

int LookupActivityHeaviest(void *pmodel, entvars_t *pev, int activity)
{
	studiohdr_t *pstudiohdr = (studiohdr_t *)pmodel;

	if (!pstudiohdr)
	{
		return 0;
	}

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);
	int weight = 0;
	int seq = ACT_INVALID;

	for (int i = 0; i < pstudiohdr->numseq; ++i)
	{
		if (pseqdesc[i].activity == activity)
		{
			if (pseqdesc[i].actweight > weight)
			{
				weight = pseqdesc[i].actweight;
				seq = i;
			}
		}
	}

	return seq;
}

NOXREF void GetEyePosition(void *pmodel, float *vecEyePosition)
{
	studiohdr_t *pstudiohdr;

	pstudiohdr = (studiohdr_t *)pmodel;

	if (!pstudiohdr)
	{
		ALERT(at_console, "GetEyePosition() Can't get pstudiohdr ptr!\n");
		return;
	}

	vecEyePosition[0] = pstudiohdr->eyeposition[0];
	vecEyePosition[1] = pstudiohdr->eyeposition[1];
	vecEyePosition[2] = pstudiohdr->eyeposition[2];
}

int LookupSequence(void *pmodel, const char *label)
{
	studiohdr_t *pstudiohdr = (studiohdr_t *)pmodel;

	if (!pstudiohdr)
	{
		return 0;
	}

	// Look up by sequence name.
	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);
	for (int i = 0; i < pstudiohdr->numseq; ++i)
	{
		if (!Q_stricmp(pseqdesc[i].label, label))
			return i;
	}

	// Not found
	return ACT_INVALID;
}

int IsSoundEvent(int eventNumber)
{
	if (eventNumber == SCRIPT_EVENT_SOUND || eventNumber == SCRIPT_EVENT_SOUND_VOICE)
	{
		return 1;
	}

	return 0;
}

NOXREF void SequencePrecache(void *pmodel, const char *pSequenceName)
{
	int index = LookupSequence(pmodel, pSequenceName);

	if (index >= 0)
	{
		studiohdr_t *pstudiohdr = (studiohdr_t *)pmodel;
		if (!pstudiohdr || index >= pstudiohdr->numseq)
		{
			return;
		}

		mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + index;
		mstudioevent_t *pevent = (mstudioevent_t *)((byte *)pstudiohdr + pseqdesc->eventindex);

		for (int i = 0; i < pseqdesc->numevents; ++i)
		{
			// Don't send client-side events to the server AI
			if (pevent[i].event >= EVENT_CLIENT)
				continue;

			// UNDONE: Add a callback to check to see if a sound is precached yet and don't allocate a copy
			// of it's name if it is.
			if (IsSoundEvent(pevent[i].event))
			{
				if (!Q_strlen(pevent[i].options))
				{
					ALERT(at_error, "Bad sound event %d in sequence %s :: %s (sound is \"%s\")\n", pevent[i].event, pstudiohdr->name, pSequenceName, pevent[i].options);
				}

				PRECACHE_SOUND((char *)(gpGlobals->pStringBase + ALLOC_STRING(pevent[i].options)));
			}
		}
	}
}

void GetSequenceInfo(void *pmodel, entvars_t *pev, float *pflFrameRate, float *pflGroundSpeed)
{
	studiohdr_t *pstudiohdr = (studiohdr_t *)pmodel;

	if (!pstudiohdr)
	{
		return;
	}

	if (pev->sequence >= pstudiohdr->numseq)
	{
		*pflFrameRate = 0;
		*pflGroundSpeed = 0;
		return;
	}

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + (int)pev->sequence;
	if (pseqdesc->numframes <= 1)
	{
		*pflFrameRate = 256.0f;
		*pflGroundSpeed = 0.0f;
		return;
	}

	*pflFrameRate = pseqdesc->fps * 256.0f / (pseqdesc->numframes - 1);
	*pflGroundSpeed = sqrt(pseqdesc->linearmovement[0] * pseqdesc->linearmovement[0] + pseqdesc->linearmovement[1] * pseqdesc->linearmovement[1] + pseqdesc->linearmovement[2] * pseqdesc->linearmovement[2]);
	*pflGroundSpeed = *pflGroundSpeed * pseqdesc->fps / (pseqdesc->numframes - 1);
}

int GetSequenceFlags(void *pmodel, entvars_t *pev)
{
	studiohdr_t *pstudiohdr = (studiohdr_t *)pmodel;

	if (!pstudiohdr || pev->sequence >= pstudiohdr->numseq)
	{
		return 0;
	}

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + (int)pev->sequence;
	return pseqdesc->flags;
}

int GetAnimationEvent(void *pmodel, entvars_t *pev, MonsterEvent_t *pMonsterEvent, float flStart, float flEnd, int index)
{
	studiohdr_t *pstudiohdr = (studiohdr_t *)pmodel;

	if (!pstudiohdr || pev->sequence >= pstudiohdr->numseq || !pMonsterEvent)
	{
		return 0;
	}

	// int events = 0;

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + (int)pev->sequence;
	mstudioevent_t *pevent = (mstudioevent_t *)((byte *)pstudiohdr + pseqdesc->eventindex);

	if (pseqdesc->numevents == 0 || index > pseqdesc->numevents)
	{
		return 0;
	}

	if (pseqdesc->numframes > 1)
	{
		flStart *= (pseqdesc->numframes - 1) / 256.0;
		flEnd *= (pseqdesc->numframes - 1) / 256.0;
	}
	else
	{
		flStart = 0;
		flEnd = 1.0;
	}

	for (; index < pseqdesc->numevents; index++)
	{
		// Don't send client-side events to the server AI
		if (pevent[index].event >= EVENT_CLIENT)
			continue;

		if ((pevent[index].frame >= flStart && pevent[index].frame < flEnd) ||
			((pseqdesc->flags & STUDIO_LOOPING)
				&& flEnd >= pseqdesc->numframes - 1
				&& pevent[index].frame < flEnd - pseqdesc->numframes + 1))
		{
			pMonsterEvent->event = pevent[index].event;
			pMonsterEvent->options = pevent[index].options;

			return index + 1;
		}
	}

	return 0;
}

float SetController(void *pmodel, entvars_t *pev, int iController, float flValue)
{
	studiohdr_t *pstudiohdr = (studiohdr_t *)pmodel;

	if (!pstudiohdr)
	{
		return flValue;
	}

	int i;
	mstudiobonecontroller_t *pbonecontroller = (mstudiobonecontroller_t *)((byte *)pstudiohdr + pstudiohdr->bonecontrollerindex);
	for (i = 0; i < pstudiohdr->numbonecontrollers; i++, pbonecontroller++)
	{
		if (pbonecontroller->index == iController)
			break;
	}

	if (i >= pstudiohdr->numbonecontrollers)
		return flValue;

	if (pbonecontroller->type & (STUDIO_XR | STUDIO_YR | STUDIO_ZR))
	{
		if (pbonecontroller->end < pbonecontroller->start)
			flValue = -flValue;

		if (pbonecontroller->end > pbonecontroller->start + 359.0)
		{
			if (flValue > 360.0)
				flValue = flValue - (int64_t)(flValue / 360.0) * 360.0;

			else if (flValue < 0.0)
				flValue = flValue + (int64_t)((flValue / -360.0) + 1) * 360.0;
		}
		else
		{
			if (flValue > ((pbonecontroller->start + pbonecontroller->end) / 2) + 180)
				flValue -= 360;

			if (flValue < ((pbonecontroller->start + pbonecontroller->end) / 2) - 180)
				flValue += 360;
		}
	}

	int setting = (int64_t)(255.0f * (flValue - pbonecontroller->start) / (pbonecontroller->end - pbonecontroller->start));

	if (setting < 0)
		setting = 0;

	if (setting > 255)
		setting = 255;

	pev->controller[ iController ] = setting;

	return setting * (1.0f / 255.0f) * (pbonecontroller->end - pbonecontroller->start) + pbonecontroller->start;
}

float SetBlending(void *pmodel, entvars_t *pev, int iBlender, float flValue)
{
	studiohdr_t *pstudiohdr = (studiohdr_t *)pmodel;
	if (!pstudiohdr)
	{
		return flValue;
	}

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + (int)pev->sequence;

	if (pseqdesc->blendtype[iBlender] == 0)
	{
		return flValue;
	}

	if (pseqdesc->blendtype[iBlender] & (STUDIO_XR | STUDIO_YR | STUDIO_ZR))
	{
		// ugly hack, invert value if end < start
		if (pseqdesc->blendend[iBlender] < pseqdesc->blendstart[iBlender])
			flValue = -flValue;

		// does the controller not wrap?
		if (pseqdesc->blendstart[iBlender] + 359.0 >= pseqdesc->blendend[iBlender])
		{
			if (flValue > ((pseqdesc->blendstart[iBlender] + pseqdesc->blendend[iBlender]) / 2.0) + 180)
			{
				flValue = flValue - 360;
			}

			if (flValue < ((pseqdesc->blendstart[iBlender] + pseqdesc->blendend[iBlender]) / 2.0) - 180)
			{
				flValue = flValue + 360;
			}
		}
	}

	int setting = (int64_t)(255.0f * (flValue - pseqdesc->blendstart[iBlender]) / (pseqdesc->blendend[iBlender] - pseqdesc->blendstart[iBlender]));

	if (setting < 0)
		setting = 0;

	if (setting > 255)
		setting = 255;

	pev->blending[iBlender] = setting;

	return setting * (1.0 / 255.0) * (pseqdesc->blendend[iBlender] - pseqdesc->blendstart[iBlender]) + pseqdesc->blendstart[iBlender];
}

int FindTransition(void *pmodel, int iEndingAnim, int iGoalAnim, int *piDir)
{
	studiohdr_t *pstudiohdr = (studiohdr_t *)pmodel;
	if (!pstudiohdr)
	{
		return iGoalAnim;
	}

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);

	// bail if we're going to or from a node 0
	if (pseqdesc[iEndingAnim].entrynode == 0 || pseqdesc[iGoalAnim].entrynode == 0)
	{
		return iGoalAnim;
	}

	int iEndNode;

	if (*piDir > 0)
	{
		iEndNode = pseqdesc[iEndingAnim].exitnode;
	}
	else
	{
		iEndNode = pseqdesc[iEndingAnim].entrynode;
	}

	if (iEndNode == pseqdesc[iGoalAnim].entrynode)
	{
		*piDir = 1;
		return iGoalAnim;
	}

	byte *pTransition = ((byte *)pstudiohdr + pstudiohdr->transitionindex);

	int iInternNode = pTransition[(iEndNode - 1)*pstudiohdr->numtransitions + (pseqdesc[iGoalAnim].entrynode - 1)];

	if (iInternNode == 0)
	{
		return iGoalAnim;
	}

	// look for someone going
	for (int i = 0; i < pstudiohdr->numseq; ++i)
	{
		if (pseqdesc[i].entrynode == iEndNode && pseqdesc[i].exitnode == iInternNode)
		{
			*piDir = 1;
			return i;
		}
		if (pseqdesc[i].nodeflags)
		{
			if (pseqdesc[i].exitnode == iEndNode && pseqdesc[i].entrynode == iInternNode)
			{
				*piDir = -1;
				return i;
			}
		}
	}

	ALERT(at_console, "error in transition graph");
	return iGoalAnim;
}

void SetBodygroup(void *pmodel, entvars_t *pev, int iGroup, int iValue)
{
	studiohdr_t *pstudiohdr = (studiohdr_t *)pmodel;
	if (!pstudiohdr)
	{
		return;
	}

	if (iGroup > pstudiohdr->numbodyparts)
	{
		return;
	}

	mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)pstudiohdr + pstudiohdr->bodypartindex) + iGroup;

	if (iValue >= pbodypart->nummodels)
	{
		return;
	}

	int iCurrent = (pev->body / pbodypart->base) % pbodypart->nummodels;
	pev->body += (iValue - iCurrent) * pbodypart->base;
}

int GetBodygroup(void *pmodel, entvars_t *pev, int iGroup)
{
	studiohdr_t *pstudiohdr = (studiohdr_t *)pmodel;

	if (!pstudiohdr || iGroup > pstudiohdr->numbodyparts)
	{
		return 0;
	}

	mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)pstudiohdr + pstudiohdr->bodypartindex) + iGroup;

	if (pbodypart->nummodels <= 1)
		return 0;

	int iCurrent = (pev->body / pbodypart->base) % pbodypart->nummodels;
	return iCurrent;
}
sv_blending_interface_t *svSavedBlending;

C_DLLEXPORT int Server_GetBlendingInterface(int version, struct sv_blending_interface_s **ppinterface, struct engine_studio_api_s *pstudio, float *rotationmatrix, float *bonetransform)
{
	if (version != SV_BLENDING_INTERFACE_VERSION)
		return 0;
    svSavedBlending = *ppinterface;
	*ppinterface = &svBlending;

	IEngineStudio.Mem_Calloc = pstudio->Mem_Calloc;
	IEngineStudio.Cache_Check = pstudio->Cache_Check;
	IEngineStudio.LoadCacheFile = pstudio->LoadCacheFile;
	IEngineStudio.Mod_Extradata = ((struct server_studio_api_s *)pstudio)->Mod_Extradata;

	g_pRotationMatrix = (float (*)[3][4])rotationmatrix;
	g_pBoneTransform = (float (*)[128][3][4])bonetransform;

	return 1;
}

void AngleQuaternion(vec_t *angles, vec_t *quaternion)
{
    /*float sy, cy, sp_, cp;
	float angle;
	float sr, cr;

	float ftmp0;
	float ftmp1;
	float ftmp2;

	angle = angles[ROLL] * 0.5;
	sy = sin(angle);
	cy = cos(angle);

	angle = angles[YAW] * 0.5;
	sp_ = sin(angle);
	cp = cos(angle);

	angle = angles[PITCH] * 0.5;
	sr = sin(angle);
	cr = cos(angle);

	ftmp0 = sr * cp;
	ftmp1 = cr * sp_;

	*quaternion = ftmp0 * cy - ftmp1 * sy;
	quaternion[1] = ftmp1 * cy + ftmp0 * sy;

	ftmp2 = cr * cp;
	quaternion[2] = ftmp2 * sy - sp_ * sr * cy;
    quaternion[3] = sp_ * sr * sy + ftmp2 * cy;*/
    float angle;
    float sr, sp, sy, cr, cp, cy;

    angle = angles[2] * 0.5;
    sy = sin(angle);
    cy = cos(angle);
    angle = angles[1] * 0.5;
    sp = sin(angle);
    cp = cos(angle);
    angle = angles[0] * 0.5;
    sr = sin(angle);
    cr = cos(angle);

    quaternion[0] = sr * cp * cy - cr * sp * sy;
    quaternion[1] = cr * sp * cy + sr * cp * sy;
    quaternion[2] = cr * cp * sy - sr * sp * cy;
    quaternion[3] = cr * cp * cy + sr * sp * sy;
}

void QuaternionSlerp(vec_t *p, vec_t *q, float t, vec_t *qt)
{
    /*int i;
	float a = 0;
	float b = 0;

	for (i = 0; i < 4; ++i)
	{
		a += (p[i] - q[i]) * (p[i] - q[i]);
		b += (p[i] + q[i]) * (p[i] + q[i]);
	}

	if (a > b)
	{
		for (i = 0; i < 4; ++i)
			q[i] = -q[i];
	}

	float sclp, sclq;
	float cosom = (p[0] * q[0] + p[1] * q[1] + p[2] * q[2] + p[3] * q[3]);

	if ((1.0 + cosom) > 0.00000001)
	{
		if ((1.0 - cosom) > 0.00000001)
		{
			float cosomega = acos((float)cosom);

			float omega = cosomega;
			float sinom = sin(cosomega);

			sclp = sin((1.0 - t) * omega) / sinom;
			sclq = sin((float)(omega * t)) / sinom;
		}
		else
		{
			sclq = t;
			sclp = 1.0 - t;
		}

		for (i = 0; i < 4; ++i)
			qt[i] = sclp * p[i] + sclq * q[i];
	}
	else
	{
		qt[0] = -q[1];
		qt[1] = q[0];
		qt[2] = -q[3];
		qt[3] = q[2];

		sclp = sin((1.0 - t) * 0.5 * M_PI);
		sclq = sin(t * 0.5 * M_PI);

		for (i = 0; i < 3; ++i)
			qt[i] = sclp * p[i] + sclq * qt[i];
    }*/
    int i;
    float omega, cosom, sinom, sclp, sclq;
    float a = 0;
    float b = 0;

    for (i = 0; i < 4; i++)
    {
        a += (p[i]-q[i]) * (p[i]-q[i]);
        b += (p[i]+q[i]) * (p[i]+q[i]);
    }

    if (a > b)
    {
        for (i = 0; i < 4; i++)
            q[i] = -q[i];
    }

    cosom = p[0] * q[0] + p[1] * q[1] + p[2] * q[2] + p[3] * q[3];

    if ((1.0 + cosom) > 0.00000001)
    {
        if ((1.0 - cosom) > 0.00000001)
        {
            omega = acos(cosom);
            sinom = sin(omega);
            sclp = sin((1.0 - t) * omega) / sinom;
            sclq = sin(t * omega) / sinom;
        }
        else
        {
            sclp = 1.0 - t;
            sclq = t;
        }

        for (i = 0; i < 4; i++)
            qt[i] = sclp * p[i] + sclq * q[i];
    }
    else
    {
        qt[0] = -p[1];
        qt[1] = p[0];
        qt[2] = -p[3];
        qt[3] = p[2];
        sclp = sin((1.0 - t) * 0.5 * M_PI);
        sclq = sin(t * 0.5 * M_PI);

        for (i = 0; i < 3; i++)
            qt[i] = sclp * p[i] + sclq * qt[i];
    }
}

void QuaternionMatrix(vec_t *quaternion, float (*matrix)[4])
{
    matrix[0][0] = 1.0 - 2.0 * quaternion[1] * quaternion[1] - 2.0 * quaternion[2] * quaternion[2];
    matrix[1][0] = 2.0 * quaternion[0] * quaternion[1] + 2.0 * quaternion[3] * quaternion[2];
    matrix[2][0] = 2.0 * quaternion[0] * quaternion[2] - 2.0 * quaternion[3] * quaternion[1];
    matrix[0][1] = 2.0 * quaternion[0] * quaternion[1] - 2.0 * quaternion[3] * quaternion[2];
    matrix[1][1] = 1.0 - 2.0 * quaternion[0] * quaternion[0] - 2.0 * quaternion[2] * quaternion[2];
    matrix[2][1] = 2.0 * quaternion[1] * quaternion[2] + 2.0 * quaternion[3] * quaternion[0];
    matrix[0][2] = 2.0 * quaternion[0] * quaternion[2] + 2.0 * quaternion[3] * quaternion[1];
    matrix[1][2] = 2.0 * quaternion[1] * quaternion[2] - 2.0 * quaternion[3] * quaternion[0];
    matrix[2][2] = 1.0 - 2.0 * quaternion[0] * quaternion[0] - 2.0 * quaternion[1] * quaternion[1];
}

mstudioanim_t *StudioGetAnim(model_t *m_pSubModel, mstudioseqdesc_t *pseqdesc)
{
	mstudioseqgroup_t *pseqgroup;
	cache_user_t *paSequences;

	pseqgroup = (mstudioseqgroup_t *)((byte *)g_pstudiohdr + g_pstudiohdr->seqgroupindex) + pseqdesc->seqgroup;

	if (pseqdesc->seqgroup == 0)
	{
		return (mstudioanim_t *)((byte *)g_pstudiohdr + pseqdesc->animindex);
	}

	paSequences = (cache_user_t *)m_pSubModel->submodels;

	if (paSequences == NULL)
	{
		paSequences = (cache_user_t *)IEngineStudio.Mem_Calloc(16, sizeof(cache_user_t)); // UNDONE: leak!
		m_pSubModel->submodels = (dmodel_t *)paSequences;
	}

	if (!IEngineStudio.Cache_Check((struct cache_user_s *)&(paSequences[ pseqdesc->seqgroup ])))
	{
		IEngineStudio.LoadCacheFile(pseqgroup->name, (struct cache_user_s *)&paSequences[ pseqdesc->seqgroup ]);
	}

	return (mstudioanim_t *)((byte *)paSequences[ pseqdesc->seqgroup ].data + pseqdesc->animindex);
}

mstudioanim_t *LookupAnimation(model_t *model, mstudioseqdesc_t *pseqdesc, int index)
{
	mstudioanim_t *panim = StudioGetAnim(model, pseqdesc);
	if (index >= 0 && index <= (pseqdesc->numblends - 1))
		panim += index * g_pstudiohdr->numbones;

	return panim;
}

void StudioCalcBoneAdj(float dadt, float *adj, const byte *pcontroller1, const byte *pcontroller2, byte mouthopen)
{
	int i, j;
	float value;
	mstudiobonecontroller_t *pbonecontroller;

	pbonecontroller = (mstudiobonecontroller_t *)((byte *)g_pstudiohdr + g_pstudiohdr->bonecontrollerindex);

	for (j = 0; j < g_pstudiohdr->numbonecontrollers; j++)
	{
		i = pbonecontroller[j].index;
		if (i <= 3)
		{
			// check for 360% wrapping
			if (pbonecontroller[j].type & STUDIO_RLOOP)
			{
				if (abs(pcontroller1[i] - pcontroller2[i]) > 128)
				{
					int a, b;
					a = (pcontroller1[j] + 128) % 256;
					b = (pcontroller2[j] + 128) % 256;
					value = ((a * dadt) + (b * (1 - dadt)) - 128) * (360.0 / 256.0) + pbonecontroller[j].start;
				}
				else
				{
					value = ((pcontroller1[i] * dadt + (pcontroller2[i]) * (1.0 - dadt))) * (360.0 / 256.0) + pbonecontroller[j].start;
				}
			}
			else
			{
				value = (pcontroller1[i] * dadt + pcontroller2[i] * (1.0 - dadt)) / 255.0;

				if (value < 0)
					value = 0;

				if (value > 1.0)
					value = 1.0;

				value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			}
		}
		else
		{
			value = mouthopen / 64.0;

			if (value > 1.0)
				value = 1.0;

			value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
		}
		switch (pbonecontroller[j].type & STUDIO_TYPES)
		{
		case STUDIO_XR:
		case STUDIO_YR:
		case STUDIO_ZR:
			adj[j] = value * (M_PI / 180.0);
			break;
		case STUDIO_X:
		case STUDIO_Y:
		case STUDIO_Z:
			adj[j] = value;
			break;
		}
	}
}

void StudioCalcBoneQuaterion(int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, float *q)
{
    vec4_t q1, q2;
    vec3_t angle1, angle2;

    for (int i = 0; i < 3; i++)
    {
        if (panim->offset[i + 3])
        {
            mstudioanimvalue_t *panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[i + 3]);
            int j = (panimvalue->num.total < panimvalue->num.valid) ? 0 : frame;

            while (panimvalue->num.total <= j)
            {
                j -= panimvalue->num.total;
                panimvalue += panimvalue->num.valid + 1;

                if (panimvalue->num.total < panimvalue->num.valid)
                    j = 0;
            }

            if (panimvalue->num.valid > j)
            {
                angle1[i] = panimvalue[j + 1].value;

                if (panimvalue->num.valid > j + 1)
                    angle2[i] = panimvalue[j + 2].value;
                else if (panimvalue->num.total > j + 1)
                    angle2[i] = angle1[i];
                else
                    angle2[i] = panimvalue[panimvalue->num.valid + 2].value;
            }
            else
            {
                angle1[i] = panimvalue[panimvalue->num.valid].value;

                if (panimvalue->num.total > j + 1)
                    angle2[i] = angle1[i];
                else
                    angle2[i] = panimvalue[panimvalue->num.valid + 2].value;
            }

            angle1[i] = pbone->value[i + 3] + angle1[i] * pbone->scale[i + 3];
            angle2[i] = pbone->value[i + 3] + angle2[i] * pbone->scale[i + 3];
        }
        else
            angle2[i] = angle1[i] = pbone->value[i + 3];

        if (pbone->bonecontroller[i + 3] != -1)
        {
            angle1[i] += adj[pbone->bonecontroller[i + 3]];
            angle2[i] += adj[pbone->bonecontroller[i + 3]];
        }
    }

    if (!VectorCompare(angle1, angle2))
    {
        AngleQuaternion(angle1, q1);
        AngleQuaternion(angle2, q2);
        QuaternionSlerp(q1, q2, s, q);
    }
    else
        AngleQuaternion(angle1, q);
/*	int j, k;
	vec4_t q1, q2;
	vec3_t angle1, angle2;
	mstudioanimvalue_t *panimvalue;

	for (j = 0; j < 3; j++)
	{
		if (panim->offset[j + 3] == 0)
		{
			// default;
			angle2[j] = angle1[j] = pbone->value[j + 3];
		}
		else
		{
			panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j + 3]);
			k = frame;

			if (panimvalue->num.total < panimvalue->num.valid)
				k = 0;

			while (panimvalue->num.total <= k)
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;

				if (panimvalue->num.total < panimvalue->num.valid)
					k = 0;
			}

			// Bah, missing blend!
			if (panimvalue->num.valid > k)
			{
				angle1[j] = panimvalue[k + 1].value;

				if (panimvalue->num.valid > k + 1)
				{
					angle2[j] = panimvalue[k + 2].value;
				}
				else
				{
					if (panimvalue->num.total > k + 1)
						angle2[j] = angle1[j];
					else
						angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
				}
			}
			else
			{
				angle1[j] = panimvalue[panimvalue->num.valid].value;
				if (panimvalue->num.total > k + 1)
				{
					angle2[j] = angle1[j];
				}
				else
				{
					angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
				}
			}
			angle1[j] = pbone->value[j + 3] + angle1[j] * pbone->scale[j + 3];
			angle2[j] = pbone->value[j + 3] + angle2[j] * pbone->scale[j + 3];
		}

		if (pbone->bonecontroller[j + 3] != -1)
		{
			angle1[j] += adj[pbone->bonecontroller[j + 3]];
			angle2[j] += adj[pbone->bonecontroller[j + 3]];
		}
	}

	if (!VectorCompare(angle1, angle2))
	{
		AngleQuaternion(angle1, q1);
		AngleQuaternion(angle2, q2);
		QuaternionSlerp(q1, q2, s, q);
	}
	else
        AngleQuaternion(angle1, q);*/
}

void StudioCalcBonePosition(int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, float *pos)
{
    /*int j, k;
	mstudioanimvalue_t *panimvalue;

	for (j = 0; j < 3; j++)
	{
		// default;
		pos[j] = pbone->value[j];
		if (panim->offset[j] != 0)
		{
			panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j]);

			k = frame;

			if (panimvalue->num.total < panimvalue->num.valid)
				k = 0;

			// find span of values that includes the frame we want
			while (panimvalue->num.total <= k)
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;

				if (panimvalue->num.total < panimvalue->num.valid)
					k = 0;
			}
			// if we're inside the span
			if (panimvalue->num.valid > k)
			{
				// and there's more data in the span
				if (panimvalue->num.valid > k + 1)
					pos[j] += (panimvalue[k + 1].value * (1.0 - s) + s * panimvalue[k + 2].value) * pbone->scale[j];
				else
					pos[j] += panimvalue[k + 1].value * pbone->scale[j];
			}
			else
			{
				// are we at the end of the repeating values section and there's another section with data?
				if (panimvalue->num.total <= k + 1)
					pos[j] += (panimvalue[panimvalue->num.valid].value * (1.0 - s) + s * panimvalue[panimvalue->num.valid + 2].value) * pbone->scale[j];

				else
					pos[j] += panimvalue[panimvalue->num.valid].value * pbone->scale[j];
			}
		}
		if (pbone->bonecontroller[j] != -1 && adj)
		{
			pos[j] += adj[pbone->bonecontroller[j]];
		}
    }*/
    for (int i = 0; i < 3; i++)
    {
        pos[i] = pbone->value[i];

        if (panim->offset[i] != 0)
        {
            mstudioanimvalue_t *panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[i]);
            int j = (panimvalue->num.total < panimvalue->num.valid) ? 0 : frame;

            while (panimvalue->num.total <= j)
            {
                j -= panimvalue->num.total;
                panimvalue += panimvalue->num.valid + 1;

                if (panimvalue->num.total < panimvalue->num.valid)
                    j = 0;
            }

            if (panimvalue->num.valid > j)
            {
                if (panimvalue->num.valid > j + 1)
                    pos[i] += (panimvalue[j + 1].value * (1 - s) + s * panimvalue[j + 2].value) * pbone->scale[i];
                else
                    pos[i] += panimvalue[j + 1].value * pbone->scale[i];
            }
            else
            {
                if (panimvalue->num.total <= j + 1)
                    pos[i] += (panimvalue[panimvalue->num.valid].value * (1 - s) + s * panimvalue[panimvalue->num.valid + 2].value) * pbone->scale[i];
                else
                    pos[i] += panimvalue[panimvalue->num.valid].value * pbone->scale[i];
            }
        }

        if (pbone->bonecontroller[i] != -1 && adj)
            pos[i] += adj[pbone->bonecontroller[i]];
    }
}

void StudioSlerpBones(vec4_t *q1, float pos1[][3], vec4_t *q2, float pos2[][3], float s)
{
    /*int i;
	vec4_t q3;
	float s1;

	if (s < 0)
		s = 0;

	else if (s > 1.0)
		s = 1.0;

	s1 = 1.0 - s;

	for (i = 0; i < g_pstudiohdr->numbones; ++i)
	{
		QuaternionSlerp(q1[i], q2[i], s, q3);

		q1[i][0] = q3[0];
		q1[i][1] = q3[1];
		q1[i][2] = q3[2];
		q1[i][3] = q3[3];

		pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * s;
		pos1[i][1] = pos1[i][1] * s1 + pos2[i][1] * s;
		pos1[i][2] = pos1[i][2] * s1 + pos2[i][2] * s;
    }*/
    if (s < 0)
        s = 0;
    else if (s > 1)
        s = 1;

    float s1 = 1 - s;

    for (int i = 0; i < g_pstudiohdr->numbones; i++)
    {
        vec4_t q3;
        QuaternionSlerp(q1[i], q2[i], s, q3);

        q1[i][0] = q3[0];
        q1[i][1] = q3[1];
        q1[i][2] = q3[2];
        q1[i][3] = q3[3];
        pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * s;
        pos1[i][1] = pos1[i][1] * s1 + pos2[i][1] * s;
        pos1[i][2] = pos1[i][2] * s1 + pos2[i][2] * s;
    }
}

void StudioCalcRotations(mstudiobone_t *pbones, int *chain, int chainlength, float *adj, float pos[128][3], vec4_t *q, mstudioseqdesc_t *pseqdesc, mstudioanim_t *panim, float f, float s)
{
	int i;
	int j;

	for (i = chainlength - 1; i >= 0; i--)
	{
		j = chain[i];

		StudioCalcBoneQuaterion((int)f, s, &pbones[j], &panim[j], adj, q[j]);
		StudioCalcBonePosition((int)f, s, &pbones[j], &panim[j], adj, pos[j]);
	}
}

void ConcatTransforms(float in1[3][4], float in2[3][4], float out[3][4])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] + in1[0][2] * in2[2][3] + in1[0][3];

	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] + in1[1][2] * in2[2][3] + in1[1][3];

	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] + in1[2][2] * in2[2][3] + in1[2][3];
}

float StudioEstimateFrame(float frame, mstudioseqdesc_t *pseqdesc)
{
	if (pseqdesc->numframes <= 1)
		return 0;

	return (float)(pseqdesc->numframes - 1) * frame / 256;
}
float GetPlayerPitch(const edict_t *pent);
float GetPlayerYaw(const edict_t *pent);
int GetPlayerGaitsequence(const edict_t *pent);
float UTIL_GetPlayerGaitYaw(int playerIndex);



void SV_NewStudioSetupBones(struct model_s *pModel, float frame, int sequence, const vec3_t angles, const vec3_t origin, const byte *pcontroller, const byte *pblending, int iBone, const edict_t *pEdict)
{
	int i, j;
	float f;
	int subframe;
	float adj[MAXSTUDIOCONTROLLERS];
	mstudiobone_t *pbones;
	mstudioseqdesc_t *pseqdesc;
	mstudioanim_t *panim;
	float bonematrix[3][4];
	int chain[MAXSTUDIOBONES];
	int chainlength;
	vec3_t temp_angles;

	static float pos[MAXSTUDIOBONES][3], pos2[MAXSTUDIOBONES][3], pos3[MAXSTUDIOBONES][3], pos4[MAXSTUDIOBONES][3];
	static vec4_t q[MAXSTUDIOBONES], q2[MAXSTUDIOBONES], q3[MAXSTUDIOBONES], q4[MAXSTUDIOBONES];

	g_pstudiohdr = (studiohdr_t *)IEngineStudio.Mod_Extradata(pModel);

	if (sequence < 0 || sequence >= g_pstudiohdr->numseq)
		sequence = 0;

	pbones = (mstudiobone_t *)((byte *)g_pstudiohdr + g_pstudiohdr->boneindex);
	pseqdesc = (mstudioseqdesc_t *)((byte *)g_pstudiohdr + g_pstudiohdr->seqindex) + sequence;
	panim = StudioGetAnim(pModel, pseqdesc);

	if (iBone < -1 || iBone >= g_pstudiohdr->numbones)
		iBone = 0;

	if (iBone == -1)
	{
		chainlength = g_pstudiohdr->numbones;

		for (i = 0; i < chainlength; i++)
			chain[(chainlength - i) - 1] = i;
	}
	else
	{
		chainlength = 0;

		for (i = iBone; i != -1; i = pbones[i].parent)
			chain[chainlength++] = i;
	}

	f = StudioEstimateFrame(frame, pseqdesc);
	subframe = f;
	f -= subframe;
	StudioCalcBoneAdj(0, adj, pcontroller, pcontroller, 0);

	for (i = chainlength - 1; i >= 0; i--)
	{
		j = chain[i];
		StudioCalcBoneQuaterion(subframe, f, &pbones[j], &panim[j], adj, q[j]);
		StudioCalcBonePosition(subframe, f, &pbones[j], &panim[j], adj, pos[j]);
	}

	if (pseqdesc->numblends == 9)
	{
		float s, t;

		s = GetPlayerYaw(pEdict);
		t = GetPlayerPitch(pEdict);

		if (s <= 127.0)
		{
			s = (s * 2.0);

			if (t <= 127.0)
			{
				t = (t * 2.0);

				for (i = chainlength - 1; i >= 0; i--)
				{
					j = chain[i];
					StudioCalcBoneQuaterion(subframe, f, &pbones[j], &panim[j], adj, q[j]);
					StudioCalcBonePosition(subframe, f, &pbones[j], &panim[j], adj, pos[j]);
				}

				panim = LookupAnimation(pModel, pseqdesc, 1);

				for (i = chainlength - 1; i >= 0; i--)
				{
					j = chain[i];
					StudioCalcBoneQuaterion(subframe, f, &pbones[j], &panim[j], adj, q2[j]);
					StudioCalcBonePosition(subframe, f, &pbones[j], &panim[j], adj, pos2[j]);
				}

				panim = LookupAnimation(pModel, pseqdesc, 3);

				for (i = chainlength - 1; i >= 0; i--)
				{
					j = chain[i];
					StudioCalcBoneQuaterion(subframe, f, &pbones[j], &panim[j], adj, q3[j]);
					StudioCalcBonePosition(subframe, f, &pbones[j], &panim[j], adj, pos3[j]);
				}

				panim = LookupAnimation(pModel, pseqdesc, 4);

				for (i = chainlength - 1; i >= 0; i--)
				{
					j = chain[i];
					StudioCalcBoneQuaterion(subframe, f, &pbones[j], &panim[j], adj, q4[j]);
					StudioCalcBonePosition(subframe, f, &pbones[j], &panim[j], adj, pos4[j]);
				}
			}
			else
			{
				t = 2.0 * (t - 127.0);

				panim = LookupAnimation(pModel, pseqdesc, 3);

				for (i = chainlength - 1; i >= 0; i--)
				{
					j = chain[i];
					StudioCalcBoneQuaterion(subframe, f, &pbones[j], &panim[j], adj, q[j]);
					StudioCalcBonePosition(subframe, f, &pbones[j], &panim[j], adj, pos[j]);
				}

				panim = LookupAnimation(pModel, pseqdesc, 4);

				for (i = chainlength - 1; i >= 0; i--)
				{
					j = chain[i];
					StudioCalcBoneQuaterion(subframe, f, &pbones[j], &panim[j], adj, q2[j]);
					StudioCalcBonePosition(subframe, f, &pbones[j], &panim[j], adj, pos2[j]);
				}

				panim = LookupAnimation(pModel, pseqdesc, 6);

				for (i = chainlength - 1; i >= 0; i--)
				{
					j = chain[i];
					StudioCalcBoneQuaterion(subframe, f, &pbones[j], &panim[j], adj, q3[j]);
					StudioCalcBonePosition(subframe, f, &pbones[j], &panim[j], adj, pos3[j]);
				}

				panim = LookupAnimation(pModel, pseqdesc, 7);

				for (i = chainlength - 1; i >= 0; i--)
				{
					j = chain[i];
					StudioCalcBoneQuaterion(subframe, f, &pbones[j], &panim[j], adj, q4[j]);
					StudioCalcBonePosition(subframe, f, &pbones[j], &panim[j], adj, pos4[j]);
				}
			}
		}
		else
		{
			s = 2.0 * (s - 127.0);

			if (t <= 127.0)
			{
				t = (t * 2.0);

				panim = LookupAnimation(pModel, pseqdesc, 1);

				for (i = chainlength - 1; i >= 0; i--)
				{
					j = chain[i];
					StudioCalcBoneQuaterion(subframe, f, &pbones[j], &panim[j], adj, q[j]);
					StudioCalcBonePosition(subframe, f, &pbones[j], &panim[j], adj, pos[j]);
				}

				panim = LookupAnimation(pModel, pseqdesc, 2);

				for (i = chainlength - 1; i >= 0; i--)
				{
					j = chain[i];
					StudioCalcBoneQuaterion(subframe, f, &pbones[j], &panim[j], adj, q2[j]);
					StudioCalcBonePosition(subframe, f, &pbones[j], &panim[j], adj, pos2[j]);
				}

				panim = LookupAnimation(pModel, pseqdesc, 4);

				for (i = chainlength - 1; i >= 0; i--)
				{
					j = chain[i];
					StudioCalcBoneQuaterion(subframe, f, &pbones[j], &panim[j], adj, q3[j]);
					StudioCalcBonePosition(subframe, f, &pbones[j], &panim[j], adj, pos3[j]);
				}

				panim = LookupAnimation(pModel, pseqdesc, 5);

				for (i = chainlength - 1; i >= 0; i--)
				{
					j = chain[i];
					StudioCalcBoneQuaterion(subframe, f, &pbones[j], &panim[j], adj, q4[j]);
					StudioCalcBonePosition(subframe, f, &pbones[j], &panim[j], adj, pos4[j]);
				}
			}
			else
			{
				t = 2.0 * (t - 127.0);

				panim = LookupAnimation(pModel, pseqdesc, 4);

				for (i = chainlength - 1; i >= 0; i--)
				{
					j = chain[i];
					StudioCalcBoneQuaterion(subframe, f, &pbones[j], &panim[j], adj, q[j]);
					StudioCalcBonePosition(subframe, f, &pbones[j], &panim[j], adj, pos[j]);
				}

				panim = LookupAnimation(pModel, pseqdesc, 5);

				for (i = chainlength - 1; i >= 0; i--)
				{
					j = chain[i];
					StudioCalcBoneQuaterion(subframe, f, &pbones[j], &panim[j], adj, q2[j]);
					StudioCalcBonePosition(subframe, f, &pbones[j], &panim[j], adj, pos2[j]);
				}

				panim = LookupAnimation(pModel, pseqdesc, 7);

				for (i = chainlength - 1; i >= 0; i--)
				{
					j = chain[i];
					StudioCalcBoneQuaterion(subframe, f, &pbones[j], &panim[j], adj, q3[j]);
					StudioCalcBonePosition(subframe, f, &pbones[j], &panim[j], adj, pos3[j]);
				}

				panim = LookupAnimation(pModel, pseqdesc, 8);

				for (i = chainlength - 1; i >= 0; i--)
				{
					j = chain[i];
					StudioCalcBoneQuaterion(subframe, f, &pbones[j], &panim[j], adj, q4[j]);
					StudioCalcBonePosition(subframe, f, &pbones[j], &panim[j], adj, pos4[j]);
				}
			}
		}

		s /= 255.0;
		t /= 255.0;

		StudioSlerpBones(q, pos, q2, pos2, s);
		StudioSlerpBones(q3, pos3, q4, pos4, s);
		StudioSlerpBones(q, pos, q3, pos3, t);
	}
	else if (pseqdesc->numblends > 1)
	{
		float s;

		pseqdesc = (mstudioseqdesc_t *)((byte *)g_pstudiohdr + g_pstudiohdr->seqindex) + sequence;
		panim = StudioGetAnim(pModel, pseqdesc);
		panim += g_pstudiohdr->numbones * 1;

		for (i = chainlength - 1; i >= 0; i--)
		{
			j = chain[i];
			StudioCalcBoneQuaterion(subframe, f, &pbones[j], &panim[j], adj, q2[j]);
			StudioCalcBonePosition(subframe, f, &pbones[j], &panim[j], adj, pos2[j]);
		}

		s = (float)pblending[0] / 255.0;
		StudioSlerpBones(q, pos, q2, pos2, s);
	}

	if (pseqdesc->numblends == 9 && sequence < ANIM_FIRST_DEATH_SEQUENCE && sequence != ANIM_SWIM_1 && sequence != ANIM_SWIM_2)
	{
		int copy = 0;
		int gaitsequence = GetPlayerGaitsequence(pEdict);

		if (gaitsequence >= g_pstudiohdr->numseq)
			gaitsequence = 0;

		if (gaitsequence < 0)
			gaitsequence = 0;

		pseqdesc = (mstudioseqdesc_t *)((byte *)g_pstudiohdr + g_pstudiohdr->seqindex) + gaitsequence;
		panim = StudioGetAnim(pModel, pseqdesc);

		for (i = chainlength - 1; i >= 0; i--)
		{
			j = chain[i];
			StudioCalcBoneQuaterion(0, 0, &pbones[j], &panim[j], adj, q2[j]);
			StudioCalcBonePosition(0, 0, &pbones[j], &panim[j], adj, pos2[j]);
		}

		for (i = 0; i < g_pstudiohdr->numbones; i++)
		{
			if (strcmp(pbones[i].name, "Bip01 Spine") == 0)
				copy = 0;
			else if (strcmp(pbones[pbones[i].parent].name, "Bip01 Pelvis") == 0)
				copy = 1;

			if (copy)
			{
				memcpy(pos[i], pos2[i], sizeof(pos[i]));
				memcpy(q[i], q2[i], sizeof(q[i]));
			}
		}
	}

	VectorCopy(angles, temp_angles);

	if (pEdict)
	{
		temp_angles[1] = UTIL_GetPlayerGaitYaw(g_engfuncs.pfnIndexOfEdict(pEdict));

		if (temp_angles[1] < 0)
			temp_angles[1] += 360;
	}

	AngleMatrix(temp_angles, (*g_pRotationMatrix));

	(*g_pRotationMatrix)[0][3] = origin[0];
	(*g_pRotationMatrix)[1][3] = origin[1];
	(*g_pRotationMatrix)[2][3] = origin[2];

	for (i = chainlength - 1; i >= 0; i--)
	{
		j = chain[i];
		QuaternionMatrix(q[j], bonematrix);

		bonematrix[0][3] = pos[j][0];
		bonematrix[1][3] = pos[j][1];
		bonematrix[2][3] = pos[j][2];

		if (pbones[j].parent == -1)
			ConcatTransforms((*g_pRotationMatrix), bonematrix, (*g_pBoneTransform)[j]);
		else
			ConcatTransforms((*g_pBoneTransform)[pbones[j].parent], bonematrix, (*g_pBoneTransform)[j]);
	}
}
void SV_OldStudioSetupBones(model_t *pModel, float frame, int sequence, const vec_t *angles, const vec_t *origin, const byte *pcontroller, const byte *pblending, int iBone, const edict_t *pEdict)
{
	int i, j;
	float f;
	float subframe;
	float adj[MAXSTUDIOCONTROLLERS];
	mstudiobone_t *pbones;
	mstudioseqdesc_t *pseqdesc;
	mstudioanim_t *panim;
	float bonematrix[3][4];
	int chain[MAXSTUDIOBONES];
	int chainlength;
	vec3_t temp_angles;
	
	/*static */float pos[MAXSTUDIOBONES][3], pos2[MAXSTUDIOBONES][3];
	/*static */float q[MAXSTUDIOBONES][4], q2[MAXSTUDIOBONES][4];

	g_pstudiohdr = (studiohdr_t *)IEngineStudio.Mod_Extradata(pModel);

	// Bound sequence number
	if (sequence < 0 || sequence >= g_pstudiohdr->numseq)
		sequence = 0;

	pbones = (mstudiobone_t *)((byte *)g_pstudiohdr + g_pstudiohdr->boneindex);
	pseqdesc = (mstudioseqdesc_t *)((byte *)g_pstudiohdr + g_pstudiohdr->seqindex) + sequence;
	panim = StudioGetAnim(pModel, pseqdesc);

	if (iBone < -1 || iBone >= g_pstudiohdr->numbones)
		iBone = 0;

	if (iBone == -1)
	{
		chainlength = g_pstudiohdr->numbones;

		for (i = 0; i < chainlength; ++i)
			chain[(chainlength - i) - 1] = i;
	}
	else
	{
		chainlength = 0;

		for (i = iBone; i != -1; i = pbones[i].parent)
			chain[chainlength++] = i;
	}

	f = StudioEstimateFrame(frame, pseqdesc);
	subframe = (int)f;
	f -= subframe;
	
	StudioCalcBoneAdj(0, adj, pcontroller, pcontroller, 0);
	StudioCalcRotations(pbones, chain, chainlength, adj, pos, q, pseqdesc, panim, subframe, f);

	if (pseqdesc->numblends != 9)
	{
		if (pseqdesc->numblends > 1)
		{
			float b = (float)pblending[0] / 255.0f;
			
			pseqdesc = (mstudioseqdesc_t *)((byte *)g_pstudiohdr + g_pstudiohdr->seqindex) + sequence;
			panim = StudioGetAnim(pModel, pseqdesc);
			panim += g_pstudiohdr->numbones;

			StudioCalcRotations(pbones, chain, chainlength, adj, pos2, q2, pseqdesc, panim, subframe, f);
			StudioSlerpBones(q, pos, q2, pos2, b);
		}
	}
	// This game knows how to do nine way blending
	else
	{
		/*static */float pos3[MAXSTUDIOBONES][3], pos4[MAXSTUDIOBONES][3];
		/*static */float q3[MAXSTUDIOBONES][4], q4[MAXSTUDIOBONES][4];
		
		float s, t;

		if( pEdict )
		{
			s = GetPlayerYaw(pEdict);
			t = GetPlayerPitch(pEdict);
		}
		else
		{
			s = t = 0.0f;
		}

		// Blending is 0-127 == Left to Middle, 128 to 255 == Middle to right
		if (s <= 127.0f)
		{
			// Scale 0-127 blending up to 0-255
			s = (s * 2.0f);

			if (t <= 127.0f)
			{
				t = (t * 2.0f);

				StudioCalcRotations(pbones, chain, chainlength, adj, pos, q, pseqdesc, panim, subframe, f);

				panim = LookupAnimation(pModel, pseqdesc, 1);
				StudioCalcRotations(pbones, chain, chainlength, adj, pos2, q2, pseqdesc, panim, subframe, f);

				panim = LookupAnimation(pModel, pseqdesc, 3);
				StudioCalcRotations(pbones, chain, chainlength, adj, pos3, q3, pseqdesc, panim, subframe, f);

				panim = LookupAnimation(pModel, pseqdesc, 4);
				StudioCalcRotations(pbones, chain, chainlength, adj, pos4, q4, pseqdesc, panim, subframe, f);
			}
			else
			{
				t = 2.0f * (t - 127.0f);

				panim = LookupAnimation(pModel, pseqdesc, 3);
				StudioCalcRotations(pbones, chain, chainlength, adj, pos, q, pseqdesc, panim, subframe, f);

				panim = LookupAnimation(pModel, pseqdesc, 4);
				StudioCalcRotations(pbones, chain, chainlength, adj, pos2, q2, pseqdesc, panim, subframe, f);

				panim = LookupAnimation(pModel, pseqdesc, 6);
				StudioCalcRotations(pbones, chain, chainlength, adj, pos3, q3, pseqdesc, panim, subframe, f);

				panim = LookupAnimation(pModel, pseqdesc, 7);
				StudioCalcRotations(pbones, chain, chainlength, adj, pos4, q4, pseqdesc, panim, subframe, f);
			}
		}
		else
		{
			// Scale 127-255 blending up to 0-255
			s = 2.0f * (s - 127.0f);

			if (t <= 127.0f)
			{
				t = (t * 2.0f);

				panim = LookupAnimation(pModel, pseqdesc, 1);
				StudioCalcRotations(pbones, chain, chainlength, adj, pos, q, pseqdesc, panim, subframe, f);

				panim = LookupAnimation(pModel, pseqdesc, 2);
				StudioCalcRotations(pbones, chain, chainlength, adj, pos2, q2, pseqdesc, panim, subframe, f);

				panim = LookupAnimation(pModel, pseqdesc, 4);
				StudioCalcRotations(pbones, chain, chainlength, adj, pos3, q3, pseqdesc, panim, subframe, f);

				panim = LookupAnimation(pModel, pseqdesc, 5);
				StudioCalcRotations(pbones, chain, chainlength, adj, pos4, q4, pseqdesc, panim, subframe, f);
			}
			else
			{
				t = 2.0f * (t - 127.0f);

				panim = LookupAnimation(pModel, pseqdesc, 4);
				StudioCalcRotations(pbones, chain, chainlength, adj, pos, q, pseqdesc, panim, subframe, f);

				panim = LookupAnimation(pModel, pseqdesc, 5);
				StudioCalcRotations(pbones, chain, chainlength, adj, pos2, q2, pseqdesc, panim, subframe, f);

				panim = LookupAnimation(pModel, pseqdesc, 7);
				StudioCalcRotations(pbones, chain, chainlength, adj, pos3, q3, pseqdesc, panim, subframe, f);

				panim = LookupAnimation(pModel, pseqdesc, 8);
				StudioCalcRotations(pbones, chain, chainlength, adj, pos4, q4, pseqdesc, panim, subframe, f);
			}
		}

		// Normalize interpolant
		s /= 255.0f;
		t /= 255.0f;

		// Spherically interpolate the bones
		StudioSlerpBones(q, pos, q2, pos2, s);
		StudioSlerpBones(q3, pos3, q4, pos4, s);
		StudioSlerpBones(q, pos, q3, pos3, t);
	}

	if (pseqdesc->numblends == 9 && sequence < ANIM_FIRST_DEATH_SEQUENCE && sequence != ANIM_SWIM_1 && sequence != ANIM_SWIM_2)
	{
		int copy = 1;
		int gaitsequence;
		
		if( pEdict )
			gaitsequence = GetPlayerGaitsequence(pEdict);	// calc gait animation
		else
			gaitsequence = 0;

		if (gaitsequence < 0 || gaitsequence >= g_pstudiohdr->numseq)
			gaitsequence = 0;

		pseqdesc = (mstudioseqdesc_t *)((byte *)g_pstudiohdr + g_pstudiohdr->seqindex) + gaitsequence;

		panim = StudioGetAnim(pModel, pseqdesc);
		StudioCalcRotations(pbones, chain, chainlength, adj, pos2, q2, pseqdesc, panim, 0, 0);

		for (i = 0; i < g_pstudiohdr->numbones; ++i)
		{
			if (!Q_strcmp(pbones[i].name, "Bip01 Spine"))
			{
				copy = 0;
			}
			else if (!Q_strcmp(pbones[pbones[i].parent].name, "Bip01 Pelvis"))
			{
				copy = 1;
			}

			if (copy)
			{
				Q_memcpy(pos[i], pos2[i], sizeof(pos[i]));
				Q_memcpy(q[i], q2[i], sizeof(q[i]));
			}
		}
	}

	VectorCopy(angles, temp_angles);

	if (pEdict != NULL && (pEdict->v.flags & (FL_CLIENT | FL_FAKECLIENT)))
	{
		temp_angles[1] = UTIL_GetPlayerGaitYaw(ENTINDEX(pEdict));

		if (temp_angles[1] < 0)
			temp_angles[1] += 360.0f;
	}

	AngleMatrix(temp_angles, (*g_pRotationMatrix));

	(*g_pRotationMatrix)[0][3] = origin[0];
	(*g_pRotationMatrix)[1][3] = origin[1];
	(*g_pRotationMatrix)[2][3] = origin[2];

	for (i = chainlength - 1; i >= 0; i--)
	{
		j = chain[i];
		QuaternionMatrix(q[j], bonematrix);

		bonematrix[0][3] = pos[j][0];
		bonematrix[1][3] = pos[j][1];
		bonematrix[2][3] = pos[j][2];

		if (pbones[j].parent == -1)
			ConcatTransforms((*g_pRotationMatrix), bonematrix, (*g_pBoneTransform)[j]);
		else
			ConcatTransforms((*g_pBoneTransform)[pbones[j].parent], bonematrix, (*g_pBoneTransform)[j]);
	}
}

void SV_StudioSetupBones(model_t *model, float frame, int sequence, const vec_t *angles, const vec_t *origin, const byte *pcontroller, const byte *pblending, int iBone, const edict_t *pEdict)
{
	if(pEdict)
	{
        if(pEdict->v.movetype == MOVETYPE_STEP || pEdict->v.movetype == MOVETYPE_WALK || (pEdict->v.movetype == MOVETYPE_FLY && pEdict->v.gaitsequence != 0))
		{
			SV_NewStudioSetupBones(model, frame, sequence, angles, origin, pcontroller, pblending, iBone, pEdict);
			return;
		}
	}

	SV_OldStudioSetupBones(model, frame, sequence, angles, origin, pcontroller, pblending, iBone, pEdict);
    svSavedBlending->SV_StudioSetupBones(model, frame, sequence, angles, origin, pcontroller, pblending, iBone, pEdict);
}

